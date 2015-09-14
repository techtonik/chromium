// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_document.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/document_resource_waiter.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/test_html_viewer_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

using mojo::AxProvider;
using mojo::View;

namespace html_viewer {
namespace {

const char kEnableTestInterface[] = "enable-html-viewer-test-interface";

bool IsTestInterfaceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTestInterface);
}

}  // namespace

// A ViewTreeDelegate implementation that delegates to a (swappable) delegate.
// This is used when one HTMLDocument takes over for another delegate
// (OnSwap()).
class ViewTreeDelegateImpl : public mojo::ViewTreeDelegate {
 public:
  explicit ViewTreeDelegateImpl(mojo::ViewTreeDelegate* delegate)
      : delegate_(delegate) {}
  ~ViewTreeDelegateImpl() override {}

  void set_delegate(mojo::ViewTreeDelegate* delegate) { delegate_ = delegate; }

 private:
  // ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override { delegate_->OnEmbed(root); }
  void OnUnembed() override { delegate_->OnUnembed(); }
  void OnConnectionLost(mojo::ViewTreeConnection* connection) override {
    delegate_->OnConnectionLost(connection);
  }

  mojo::ViewTreeDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeDelegateImpl);
};

HTMLDocument::BeforeLoadCache::BeforeLoadCache() {}

HTMLDocument::BeforeLoadCache::~BeforeLoadCache() {
  STLDeleteElements(&ax_provider_requests);
  STLDeleteElements(&test_interface_requests);
}

HTMLDocument::TransferableState::TransferableState()
    : owns_view_tree_connection(false), root(nullptr) {}

HTMLDocument::TransferableState::~TransferableState() {}

void HTMLDocument::TransferableState::Move(TransferableState* other) {
  owns_view_tree_connection = other->owns_view_tree_connection;
  root = other->root;
  view_tree_delegate_impl = other->view_tree_delegate_impl.Pass();

  other->root = nullptr;
  other->owns_view_tree_connection = false;
}

HTMLDocument::HTMLDocument(mojo::ApplicationImpl* html_document_app,
                           mojo::ApplicationConnection* connection,
                           mojo::URLResponsePtr response,
                           GlobalState* global_state,
                           const DeleteCallback& delete_callback,
                           HTMLFactory* factory)
    : app_refcount_(html_document_app->app_lifetime_helper()
                        ->CreateAppRefCount()),
      html_document_app_(html_document_app),
      connection_(connection),
      global_state_(global_state),
      frame_(nullptr),
      delete_callback_(delete_callback),
      factory_(factory) {
  connection->AddService<web_view::FrameTreeClient>(this);
  connection->AddService<AxProvider>(this);
  connection->AddService<mojo::ViewTreeClient>(this);
  connection->AddService<devtools_service::DevToolsAgent>(this);
  if (IsTestInterfaceEnabled())
    connection->AddService<TestHTMLViewer>(this);

  resource_waiter_.reset(
      new DocumentResourceWaiter(global_state_, response.Pass(), this));
}

void HTMLDocument::Destroy() {
  if (resource_waiter_) {
    mojo::View* root = resource_waiter_->root();
    if (root) {
      resource_waiter_.reset();
      delete root->connection();
    } else {
      delete this;
    }
  } else if (frame_) {
    // Closing the frame ends up destroying the ViewManager, which triggers
    // deleting this (OnConnectionLost()).
    frame_->Close();
  } else if (transferable_state_.root) {
    // This triggers deleting us.
    if (transferable_state_.owns_view_tree_connection)
      delete transferable_state_.root->connection();
    else
      delete this;
  } else {
    delete this;
  }
}

HTMLDocument::~HTMLDocument() {
  delete_callback_.Run(this);

  STLDeleteElements(&ax_providers_);
}

void HTMLDocument::Load() {
  DCHECK(resource_waiter_ && resource_waiter_->is_ready());

  // Note: |view| is null if we're taking over for an existing frame.
  mojo::View* view = resource_waiter_->root();
  if (view) {
    global_state_->InitIfNecessary(
        view->viewport_metrics().size_in_pixels.To<gfx::Size>(),
        view->viewport_metrics().device_pixel_ratio);
  }

  scoped_ptr<WebURLRequestExtraData> extra_data(new WebURLRequestExtraData);
  extra_data->synthetic_response =
      resource_waiter_->ReleaseURLResponse().Pass();

  frame_ = HTMLFrameTreeManager::CreateFrameAndAttachToTree(
      global_state_, view, resource_waiter_.Pass(), this);

  // If the frame wasn't created we can destroy ourself.
  if (!frame_) {
    Destroy();
    return;
  }

  if (devtools_agent_request_.is_pending()) {
    if (frame_->devtools_agent()) {
      frame_->devtools_agent()->BindToRequest(devtools_agent_request_.Pass());
    } else {
      devtools_agent_request_ =
          mojo::InterfaceRequest<devtools_service::DevToolsAgent>();
    }
  }

  const GURL url(extra_data->synthetic_response->url);

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data.release());

  frame_->web_frame()->toWebLocalFrame()->loadRequest(web_request);
}

HTMLDocument::BeforeLoadCache* HTMLDocument::GetBeforeLoadCache() {
  CHECK(!did_finish_local_frame_load_);
  if (!before_load_cache_.get())
    before_load_cache_.reset(new BeforeLoadCache);
  return before_load_cache_.get();
}

void HTMLDocument::OnEmbed(View* root) {
  transferable_state_.root = root;
  resource_waiter_->SetRoot(root);
}

void HTMLDocument::OnConnectionLost(mojo::ViewTreeConnection* connection) {
  delete this;
}

void HTMLDocument::OnFrameDidFinishLoad() {
  did_finish_local_frame_load_ = true;
  scoped_ptr<BeforeLoadCache> before_load_cache = before_load_cache_.Pass();
  if (!before_load_cache)
    return;

  // Bind any pending AxProvider and TestHTMLViewer interface requests.
  for (auto it : before_load_cache->ax_provider_requests) {
    ax_providers_.insert(new AxProviderImpl(
        frame_->frame_tree_manager()->GetWebView(), it->Pass()));
  }
  for (auto it : before_load_cache->test_interface_requests) {
    CHECK(IsTestInterfaceEnabled());
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), it->Pass()));
  }
}

mojo::ApplicationImpl* HTMLDocument::GetApp() {
  return html_document_app_;
}

HTMLFactory* HTMLDocument::GetHTMLFactory() {
  return factory_;
}

void HTMLDocument::OnFrameSwappedToRemote() {
  // When the frame becomes remote HTMLDocument is no longer needed.
  frame_ = nullptr;
  Destroy();
}

void HTMLDocument::OnSwap(HTMLFrame* frame, HTMLFrameDelegate* old_delegate) {
  DCHECK(frame->IsLocal());
  DCHECK(frame->view());
  DCHECK(!frame_);
  DCHECK(!transferable_state_.root);
  if (!old_delegate) {
    // We're taking over a child of a local root that isn't associated with a
    // delegate. In this case the frame's view is not the root of the
    // ViewTreeConnection.
    transferable_state_.owns_view_tree_connection = false;
    transferable_state_.root = frame->view();
  } else {
    HTMLDocument* old_document = static_cast<HTMLDocument*>(old_delegate);
    transferable_state_.Move(&old_document->transferable_state_);
    if (transferable_state_.view_tree_delegate_impl)
      transferable_state_.view_tree_delegate_impl->set_delegate(this);
    old_document->frame_ = nullptr;
    old_document->Destroy();
  }
}

void HTMLDocument::OnFrameDestroyed() {
  if (!transferable_state_.owns_view_tree_connection)
    delete this;
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<AxProvider> request) {
  if (!did_finish_local_frame_load_) {
    // Cache AxProvider interface requests until the document finishes loading.
    auto cached_request = new mojo::InterfaceRequest<AxProvider>();
    *cached_request = request.Pass();
    GetBeforeLoadCache()->ax_provider_requests.insert(cached_request);
  } else {
    ax_providers_.insert(
        new AxProviderImpl(frame_->web_view(), request.Pass()));
  }
}

void HTMLDocument::Create(mojo::ApplicationConnection* connection,
                          mojo::InterfaceRequest<TestHTMLViewer> request) {
  CHECK(IsTestInterfaceEnabled());
  if (!did_finish_local_frame_load_) {
    auto cached_request = new mojo::InterfaceRequest<TestHTMLViewer>();
    *cached_request = request.Pass();
    GetBeforeLoadCache()->test_interface_requests.insert(cached_request);
  } else {
    test_html_viewers_.push_back(new TestHTMLViewerImpl(
        frame_->web_frame()->toWebLocalFrame(), request.Pass()));
  }
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<web_view::FrameTreeClient> request) {
  if (frame_) {
    DVLOG(1) << "Request for FrameTreeClient after one already vended.";
    return;
  }
  resource_waiter_->Bind(request.Pass());
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<devtools_service::DevToolsAgent> request) {
  if (frame_) {
    if (frame_->devtools_agent())
      frame_->devtools_agent()->BindToRequest(request.Pass());
  } else {
    devtools_agent_request_ = request.Pass();
  }
}

void HTMLDocument::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ViewTreeClient> request) {
  DCHECK(!transferable_state_.view_tree_delegate_impl);
  transferable_state_.view_tree_delegate_impl.reset(
      new ViewTreeDelegateImpl(this));
  transferable_state_.owns_view_tree_connection = true;
  mojo::ViewTreeConnection::Create(
      transferable_state_.view_tree_delegate_impl.get(), request.Pass());
}

}  // namespace html_viewer
