// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_text_input_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/devtools_agent_impl.h"
#include "components/html_viewer/geolocation_client_impl.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_factory.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_frame_properties.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/html_widget.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/stats_collection_controller.h"
#include "components/html_viewer/touch_handler.h"
#include "components/html_viewer/web_layer_impl.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/mus/ids.h"
#include "components/mus/public/cpp/scoped_view_ptr.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mus::View;
using web_view::HTMLMessageEvent;
using web_view::HTMLMessageEventPtr;

namespace html_viewer {
namespace {

const size_t kMaxTitleChars = 4 * 1024;

web_view::NavigationTargetType WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return web_view::NAVIGATION_TARGET_TYPE_EXISTING_FRAME;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return web_view::NAVIGATION_TARGET_TYPE_NEW_FRAME;
    default:
      return web_view::NAVIGATION_TARGET_TYPE_NO_PREFERENCE;
  }
}

HTMLFrame* GetPreviousSibling(HTMLFrame* frame) {
  DCHECK(frame->parent());
  auto iter = std::find(frame->parent()->children().begin(),
                        frame->parent()->children().end(), frame);
  return (iter == frame->parent()->children().begin()) ? nullptr : *(--iter);
}

}  // namespace

HTMLFrame::HTMLFrame(CreateParams* params)
    : frame_tree_manager_(params->manager),
      parent_(params->parent),
      view_(nullptr),
      id_(params->id),
      web_frame_(nullptr),
      delegate_(params->delegate),
      weak_factory_(this) {
  if (parent_)
    parent_->children_.push_back(this);

  if (params->view && params->view->id() == id_)
    SetView(params->view);

  SetReplicatedFrameStateFromClientProperties(params->properties, &state_);

  if (!parent_) {
    CreateRootWebWidget();

    // This is the root of the tree (aka the main frame).
    // Expected order for creating webframes is:
    // . Create local webframe (first webframe must always be local).
    // . Set as main frame on WebView.
    // . Swap to remote (if not local).
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    // We need to set the main frame before creating children so that state is
    // properly set up in blink.
    web_view()->setMainFrame(local_web_frame);

    // The resize and setDeviceScaleFactor() needs to be after setting the main
    // frame.
    const gfx::Size size_in_pixels(params->view->bounds().width,
                                   params->view->bounds().height);
    const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
        params->view->viewport_metrics().device_pixel_ratio, size_in_pixels);
    web_view()->resize(size_in_dips);
    web_frame_ = local_web_frame;
    web_view()->setDeviceScaleFactor(global_state()->device_pixel_ratio());
    if (id_ != params->view->id()) {
      blink::WebRemoteFrame* remote_web_frame =
          blink::WebRemoteFrame::create(state_.tree_scope, this);
      local_web_frame->swap(remote_web_frame);
      web_frame_ = remote_web_frame;
    } else {
      // Setup a DevTools agent if this is the local main frame and the browser
      // side has set relevant client properties.
      mojo::Array<uint8_t> devtools_id =
          GetValueFromClientProperties("devtools-id", params->properties);
      if (!devtools_id.is_null()) {
        mojo::Array<uint8_t> devtools_state =
            GetValueFromClientProperties("devtools-state", params->properties);
        std::string devtools_state_str = devtools_state.To<std::string>();
        devtools_agent_.reset(new DevToolsAgentImpl(
            web_frame_->toWebLocalFrame(), devtools_id.To<std::string>(),
            devtools_state.is_null() ? nullptr : &devtools_state_str));
      }

      // Collect startup perf data for local main frames in test environments.
      // Child frames aren't tracked, and tracking remote frames is redundant.
      startup_performance_data_collector_ =
          StatsCollectionController::Install(web_frame_, GetLocalRootApp());
    }
  } else if (!params->allow_local_shared_frame && params->view &&
             id_ == params->view->id()) {
    // Frame represents the local frame, and it isn't the root of the tree.
    HTMLFrame* previous_sibling = GetPreviousSibling(this);
    blink::WebFrame* previous_web_frame =
        previous_sibling ? previous_sibling->web_frame() : nullptr;
    CHECK(!parent_->IsLocal());
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createLocalChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this,
        previous_web_frame);
    CreateLocalRootWebWidget(web_frame_->toWebLocalFrame());
  } else if (!parent_->IsLocal()) {
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createRemoteChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this);
  } else {
    // TODO(sky): this DCHECK, and |allow_local_shared_frame| should be
    // moved to HTMLFrameTreeManager. It makes more sense there.
    // This should never happen (if we create a local child we don't call
    // Init(), and the frame server should not being creating child frames of
    // this frame).
    DCHECK(params->allow_local_shared_frame);

    blink::WebLocalFrame* child_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    web_frame_ = child_web_frame;
    parent_->web_frame_->appendChild(child_web_frame);
  }

  if (!IsLocal()) {
    blink::WebRemoteFrame* remote_web_frame = web_frame_->toWebRemoteFrame();
    if (remote_web_frame) {
      remote_web_frame->setReplicatedOrigin(state_.origin);
      remote_web_frame->setReplicatedName(state_.name);
    }
  }
}

void HTMLFrame::Close() {
  if (GetWebWidget()) {
    // Closing the root widget (WebView) implicitly detaches. For children
    // (which have a WebFrameWidget) a detach() is required. Use a temporary
    // as if 'this' is the root the call to GetWebWidget()->close() deletes
    // 'this'.
    const bool is_child = parent_ != nullptr;
    GetWebWidget()->close();
    if (is_child)
      web_frame_->detach();
  } else {
    web_frame_->detach();
  }
}

const HTMLFrame* HTMLFrame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const HTMLFrame* child : children_) {
    const HTMLFrame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

blink::WebView* HTMLFrame::web_view() {
  blink::WebWidget* web_widget =
      html_widget_ ? html_widget_->GetWidget() : nullptr;
  return web_widget && web_widget->isWebView()
             ? static_cast<blink::WebView*>(web_widget)
             : nullptr;
}

blink::WebWidget* HTMLFrame::GetWebWidget() {
  return html_widget_ ? html_widget_->GetWidget() : nullptr;
}

bool HTMLFrame::IsLocal() const {
  return web_frame_->isWebLocalFrame();
}

bool HTMLFrame::HasLocalDescendant() const {
  if (IsLocal())
    return true;

  for (HTMLFrame* child : children_) {
    if (child->HasLocalDescendant())
      return true;
  }
  return false;
}

HTMLFrame::~HTMLFrame() {
  DCHECK(children_.empty());

  if (parent_) {
    auto iter =
        std::find(parent_->children_.begin(), parent_->children_.end(), this);
    parent_->children_.erase(iter);
  }
  parent_ = nullptr;

  frame_tree_manager_->OnFrameDestroyed(this);

  if (delegate_)
    delegate_->OnFrameDestroyed();

  if (view_) {
    view_->RemoveObserver(this);
    mus::ScopedViewPtr::DeleteViewOrViewManager(view_);
  }
}

blink::WebMediaPlayer* HTMLFrame::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return global_state()->media_factory()->CreateMediaPlayer(
      frame, url, client, encrypted_client, initial_cdm,
      GetLocalRootApp()->shell());
}

blink::WebFrame* HTMLFrame::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& frame_name,
    blink::WebSandboxFlags sandbox_flags) {
  DCHECK(IsLocal());  // Can't create children of remote frames.
  DCHECK_EQ(parent, web_frame_);
  DCHECK(view_);  // If we're local we have to have a view.
  // Create the view that will house the frame now. We embed once we know the
  // url (see decidePolicyForNavigation()).
  mus::View* child_view = view_->connection()->CreateView();
  ReplicatedFrameState child_state;
  child_state.name = frame_name;
  child_state.tree_scope = scope;
  child_state.sandbox_flags = sandbox_flags;
  mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
  client_properties.mark_non_null();
  ClientPropertiesFromReplicatedFrameState(child_state, &client_properties);

  child_view->SetVisible(true);
  view_->AddChild(child_view);

  GetLocalRoot()->server_->OnCreatedFrame(id_, child_view->id(),
                                          client_properties.Pass());

  HTMLFrame::CreateParams params(frame_tree_manager_, this, child_view->id(),
                                 child_view, client_properties, nullptr);
  params.allow_local_shared_frame = true;
  HTMLFrame* child_frame =
      GetLocalRoot()->delegate_->GetHTMLFactory()->CreateHTMLFrame(&params);
  child_frame->owned_view_.reset(new mus::ScopedViewPtr(child_view));
  return child_frame->web_frame_;
}

void HTMLFrame::frameDetached(blink::WebFrame* web_frame,
                              blink::WebFrameClient::DetachType type) {
  if (type == blink::WebFrameClient::DetachType::Swap) {
    web_frame->close();
    return;
  }

  DCHECK(type == blink::WebFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame);
}

blink::WebCookieJar* HTMLFrame::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLFrame::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (info.urlRequest.extraData()) {
    DCHECK_EQ(blink::WebNavigationPolicyCurrentTab, info.defaultPolicy);
    return blink::WebNavigationPolicyCurrentTab;
  }

  // about:blank is treated as the same origin and is always allowed for
  // frames.
  if (parent_ && info.urlRequest.url() == GURL(url::kAboutBlankURL) &&
      info.defaultPolicy == blink::WebNavigationPolicyCurrentTab) {
    return blink::WebNavigationPolicyCurrentTab;
  }

  // Ask the FrameTreeServer to handle the navigation. By returning
  // WebNavigationPolicyIgnore the load is suppressed.
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
  GetLocalRoot()->server_->RequestNavigate(
      WebNavigationPolicyToNavigationTarget(info.defaultPolicy), id_,
      url_request.Pass());
  return blink::WebNavigationPolicyIgnore;
}

void HTMLFrame::didHandleOnloadEvents(blink::WebLocalFrame* frame) {
  static bool recorded = false;
  if (!recorded && startup_performance_data_collector_) {
    startup_performance_data_collector_->SetFirstWebContentsMainFrameLoadTime(
        base::Time::Now().ToInternalValue());
    recorded = true;
  }
}

void HTMLFrame::didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                       const blink::WebString& source_name,
                                       unsigned source_line,
                                       const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void HTMLFrame::didFinishLoad(blink::WebLocalFrame* frame) {
  if (GetLocalRoot() == this)
    delegate_->OnFrameDidFinishLoad();
}

void HTMLFrame::didNavigateWithinPage(blink::WebLocalFrame* frame,
                                      const blink::WebHistoryItem& history_item,
                                      blink::WebHistoryCommitType commit_type) {
  GetLocalRoot()->server_->DidNavigateLocally(id_,
                                              history_item.urlString().utf8());
}

blink::WebGeolocationClient* HTMLFrame::geolocationClient() {
  if (!geolocation_client_impl_)
    geolocation_client_impl_.reset(new GeolocationClientImpl);
  return geolocation_client_impl_.get();
}

blink::WebEncryptedMediaClient* HTMLFrame::encryptedMediaClient() {
  return global_state()->media_factory()->GetEncryptedMediaClient();
}

void HTMLFrame::didStartLoading(bool to_different_document) {
  GetLocalRoot()->server_->LoadingStarted(id_);
}

void HTMLFrame::didStopLoading() {
  GetLocalRoot()->server_->LoadingStopped(id_);
}

void HTMLFrame::didChangeLoadProgress(double load_progress) {
  GetLocalRoot()->server_->ProgressChanged(id_, load_progress);
}

void HTMLFrame::didChangeName(blink::WebLocalFrame* frame,
                              const blink::WebString& name) {
  state_.name = name;
  GetLocalRoot()->server_->SetClientProperty(id_, kPropertyFrameName,
                                             FrameNameToClientProperty(name));
}

void HTMLFrame::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  state_.origin = FrameOrigin(frame);
  GetLocalRoot()->server_->SetClientProperty(
      id_, kPropertyFrameOrigin, FrameOriginToClientProperty(frame));
}

void HTMLFrame::didReceiveTitle(blink::WebLocalFrame* frame,
                                const blink::WebString& title,
                                blink::WebTextDirection direction) {
  // TODO(beng): handle |direction|.
  mojo::String formatted;
  if (!title.isNull()) {
    formatted =
        mojo::String::From(base::string16(title).substr(0, kMaxTitleChars));
  }
  GetLocalRoot()->server_->TitleChanged(id_, formatted);
}

void HTMLFrame::Bind(web_view::FrameTreeServerPtr frame_tree_server,
                     mojo::InterfaceRequest<web_view::FrameTreeClient>
                         frame_tree_client_request) {
  DCHECK(IsLocal());
  server_ = frame_tree_server.Pass();
  server_.set_connection_error_handler(
      base::Bind(&HTMLFrame::Close, base::Unretained(this)));
  frame_tree_client_binding_.reset(new mojo::Binding<web_view::FrameTreeClient>(
      this, frame_tree_client_request.Pass()));
}

void HTMLFrame::SetValueFromClientProperty(const std::string& name,
                                           mojo::Array<uint8_t> new_data) {
  if (IsLocal())
    return;

  // Only the name and origin dynamically change.
  if (name == kPropertyFrameOrigin) {
    state_.origin = FrameOriginFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedOrigin(state_.origin);
  } else if (name == kPropertyFrameName) {
    state_.name = FrameNameFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedName(state_.name);
  }
}

HTMLFrame* HTMLFrame::GetLocalRoot() {
  HTMLFrame* frame = this;
  while (frame && !frame->delegate_)
    frame = frame->parent_;
  return frame;
}

mojo::ApplicationImpl* HTMLFrame::GetLocalRootApp() {
  return GetLocalRoot()->delegate_->GetApp();
}

web_view::FrameTreeServer* HTMLFrame::GetFrameTreeServer() {
  // Prefer the local root.
  HTMLFrame* local_root = GetLocalRoot();
  if (local_root)
    return local_root->server_.get();

  // No local root. This means we're a remote frame with no local frame
  // ancestors. Use the local frame from the FrameTreeServer.
  return frame_tree_manager_->local_root_->server_.get();
}

void HTMLFrame::SetView(mus::View* view) {
  if (view_)
    view_->RemoveObserver(this);
  view_ = view;
  if (view_)
    view_->AddObserver(this);
}

void HTMLFrame::CreateRootWebWidget() {
  DCHECK(!html_widget_);
  if (view_) {
    HTMLWidgetRootLocal::CreateParams create_params(GetLocalRootApp(),
                                                    global_state(), view_);
    html_widget_.reset(
        delegate_->GetHTMLFactory()->CreateHTMLWidgetRootLocal(&create_params));
  } else {
    html_widget_.reset(new HTMLWidgetRootRemote);
  }
}

void HTMLFrame::CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame) {
  DCHECK(!html_widget_);
  DCHECK(IsLocal());
  html_widget_.reset(new HTMLWidgetLocalRoot(GetLocalRootApp(), global_state(),
                                             view_, local_frame));
}

void HTMLFrame::UpdateFocus() {
  blink::WebWidget* web_widget = GetWebWidget();
  if (!web_widget || !view_)
    return;
  const bool is_focused = view_ && view_->HasFocus();
  web_widget->setFocus(is_focused);
  if (web_widget->isWebView())
    static_cast<blink::WebView*>(web_widget)->setIsActive(is_focused);
}

void HTMLFrame::SwapToRemote() {
  DCHECK(IsLocal());

  HTMLFrameDelegate* delegate = delegate_;
  delegate_ = nullptr;

  blink::WebRemoteFrame* remote_frame =
      blink::WebRemoteFrame::create(state_.tree_scope, this);
  remote_frame->initializeFromFrame(web_frame_->toWebLocalFrame());
  // swap() ends up calling us back and we then close the frame, which deletes
  // it.
  web_frame_->swap(remote_frame);
  // TODO(sky): this isn't quite right, but WebLayerImpl is temporary.
  if (owned_view_) {
    web_layer_.reset(
        new WebLayerImpl(owned_view_->view(),
                         global_state()->device_pixel_ratio()));
  }
  remote_frame->setRemoteWebLayer(web_layer_.get());
  remote_frame->setReplicatedName(state_.name);
  remote_frame->setReplicatedOrigin(state_.origin);
  remote_frame->setReplicatedSandboxFlags(state_.sandbox_flags);
  web_frame_ = remote_frame;
  SetView(nullptr);
  if (delegate)
    delegate->OnFrameSwappedToRemote();
}

void HTMLFrame::SwapToLocal(
    HTMLFrameDelegate* delegate,
    mus::View* view,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  CHECK(!IsLocal());
  // It doesn't make sense for the root to swap to local.
  CHECK(parent_);
  delegate_ = delegate;
  SetView(view);
  SetReplicatedFrameStateFromClientProperties(properties, &state_);
  blink::WebLocalFrame* local_web_frame =
      blink::WebLocalFrame::create(state_.tree_scope, this);
  local_web_frame->initializeToReplaceRemoteFrame(
      web_frame_->toWebRemoteFrame(), state_.name, state_.sandbox_flags);
  // The swap() ends up calling to frameDetached() and deleting the old.
  web_frame_->swap(local_web_frame);
  web_frame_ = local_web_frame;

  web_layer_.reset();
}

void HTMLFrame::SwapDelegate(HTMLFrameDelegate* delegate) {
  DCHECK(IsLocal());
  HTMLFrameDelegate* old_delegate = delegate_;
  delegate_ = delegate;
  delegate->OnSwap(this, old_delegate);
}

HTMLFrame* HTMLFrame::FindFrameWithWebFrame(blink::WebFrame* web_frame) {
  if (web_frame_ == web_frame)
    return this;
  for (HTMLFrame* child_frame : children_) {
    HTMLFrame* result = child_frame->FindFrameWithWebFrame(web_frame);
    if (result)
      return result;
  }
  return nullptr;
}

void HTMLFrame::FrameDetachedImpl(blink::WebFrame* web_frame) {
  DCHECK_EQ(web_frame_, web_frame);

  while (!children_.empty()) {
    HTMLFrame* child = children_.front();
    child->Close();
    DCHECK(children_.empty() || children_.front() != child);
  }

  if (web_frame->parent())
    web_frame->parent()->removeChild(web_frame);

  delete this;
}

void HTMLFrame::OnViewBoundsChanged(View* view,
                                    const Rect& old_bounds,
                                    const Rect& new_bounds) {
  DCHECK_EQ(view, view_);
  if (html_widget_)
    html_widget_->OnViewBoundsChanged(view);
}

void HTMLFrame::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
  Close();
}

void HTMLFrame::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  if (event->pointer_data) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->x /= global_state()->device_pixel_ratio();
    event->pointer_data->y /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_x /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_y /= global_state()->device_pixel_ratio();
  }

  blink::WebWidget* web_widget = GetWebWidget();

  if (!touch_handler_ && web_widget)
    touch_handler_.reset(new TouchHandler(web_widget));

  if (touch_handler_ && (event->action == mojo::EVENT_TYPE_POINTER_DOWN ||
                         event->action == mojo::EVENT_TYPE_POINTER_UP ||
                         event->action == mojo::EVENT_TYPE_POINTER_CANCEL ||
                         event->action == mojo::EVENT_TYPE_POINTER_MOVE) &&
      event->pointer_data->kind == mojo::POINTER_KIND_TOUCH) {
    touch_handler_->OnTouchEvent(*event);
    return;
  }

  if (!web_widget)
    return;

  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_widget->handleInputEvent(*web_event);
}

void HTMLFrame::OnViewFocusChanged(mus::View* gained_focus,
                                   mus::View* lost_focus) {
  UpdateFocus();
}

void HTMLFrame::OnConnect(web_view::FrameTreeServerPtr server,
                          uint32_t change_id,
                          uint32_t view_id,
                          web_view::ViewConnectType view_connect_type,
                          mojo::Array<web_view::FrameDataPtr> frame_data,
                          const OnConnectCallback& callback) {
  // OnConnect() is only sent once, and has been received (by
  // DocumentResourceWaiter) by the time we get here.
  NOTREACHED();
}

void HTMLFrame::OnFrameAdded(uint32_t change_id,
                             web_view::FrameDataPtr frame_data) {
  frame_tree_manager_->ProcessOnFrameAdded(this, change_id, frame_data.Pass());
}

void HTMLFrame::OnFrameRemoved(uint32_t change_id, uint32_t frame_id) {
  frame_tree_manager_->ProcessOnFrameRemoved(this, change_id, frame_id);
}

void HTMLFrame::OnFrameClientPropertyChanged(uint32_t frame_id,
                                             const mojo::String& name,
                                             mojo::Array<uint8_t> new_value) {
  frame_tree_manager_->ProcessOnFrameClientPropertyChanged(this, frame_id, name,
                                                           new_value.Pass());
}

void HTMLFrame::OnPostMessageEvent(uint32_t source_frame_id,
                                   uint32_t target_frame_id,
                                   HTMLMessageEventPtr serialized_event) {
  NOTIMPLEMENTED();  // For message ports.

  HTMLFrame* target = frame_tree_manager_->root_->FindFrame(target_frame_id);
  HTMLFrame* source = frame_tree_manager_->root_->FindFrame(source_frame_id);
  if (!target || !source) {
    DVLOG(1) << "Invalid source or target for PostMessage";
    return;
  }

  if (!target->IsLocal()) {
    DVLOG(1) << "Target for PostMessage is not lot local";
    return;
  }

  blink::WebLocalFrame* target_web_frame =
      target->web_frame_->toWebLocalFrame();

  blink::WebSerializedScriptValue serialized_script_value;
  serialized_script_value = blink::WebSerializedScriptValue::fromString(
      serialized_event->data.To<blink::WebString>());

  blink::WebMessagePortChannelArray channels;

  // Create an event with the message.  The next-to-last parameter to
  // initMessageEvent is the last event ID, which is not used with postMessage.
  blink::WebDOMEvent event =
      target_web_frame->document().createEvent("MessageEvent");
  blink::WebDOMMessageEvent msg_event = event.to<blink::WebDOMMessageEvent>();
  msg_event.initMessageEvent(
      "message",
      // |canBubble| and |cancellable| are always false
      false, false, serialized_script_value,
      serialized_event->source_origin.To<blink::WebString>(),
      source->web_frame_, target_web_frame->document(), "", channels);

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  blink::WebSecurityOrigin target_origin;
  if (!serialized_event->target_origin.is_null()) {
    target_origin = blink::WebSecurityOrigin::createFromString(
        serialized_event->target_origin.To<blink::WebString>());
  }
  target_web_frame->dispatchMessageEventWithOriginCheck(target_origin,
                                                        msg_event);
}

void HTMLFrame::OnWillNavigate(uint32_t target_frame_id) {
  HTMLFrame* target = frame_tree_manager_->root_->FindFrame(target_frame_id);
  if (target && target->IsLocal() &&
      target != frame_tree_manager_->local_root_) {
    target->SwapToRemote();
  }
}

void HTMLFrame::frameDetached(blink::WebRemoteFrameClient::DetachType type) {
  if (type == blink::WebRemoteFrameClient::DetachType::Swap) {
    web_frame_->close();
    return;
  }

  DCHECK(type == blink::WebRemoteFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame_);
}

void HTMLFrame::postMessageEvent(blink::WebLocalFrame* source_web_frame,
                                 blink::WebRemoteFrame* target_web_frame,
                                 blink::WebSecurityOrigin target_origin,
                                 blink::WebDOMMessageEvent web_event) {
  NOTIMPLEMENTED();  // message_ports aren't implemented yet.

  HTMLFrame* source_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(source_web_frame);
  DCHECK(source_frame);
  HTMLFrame* target_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(target_web_frame);
  DCHECK(target_frame);

  HTMLMessageEventPtr event(HTMLMessageEvent::New());
  event->data = mojo::Array<uint8_t>::From(web_event.data().toString());
  event->source_origin = mojo::String::From(web_event.origin());
  if (!target_origin.isNull())
    event->target_origin = mojo::String::From(target_origin.toString());

  GetFrameTreeServer()->PostMessageEventToFrame(
      source_frame->id_, target_frame->id_, event.Pass());
}

void HTMLFrame::initializeChildFrame(const blink::WebRect& frame_rect,
                                     float scale_factor) {
  // NOTE: |scale_factor| is always 1.
  const gfx::Rect rect_in_dip(frame_rect.x, frame_rect.y, frame_rect.width,
                              frame_rect.height);
  const gfx::Rect rect_in_pixels(gfx::ConvertRectToPixel(
      global_state()->device_pixel_ratio(), rect_in_dip));
  const mojo::RectPtr mojo_rect_in_pixels(mojo::Rect::From(rect_in_pixels));
  view_->SetBounds(*mojo_rect_in_pixels);
}

void HTMLFrame::navigate(const blink::WebURLRequest& request,
                         bool should_replace_current_entry) {
  // TODO: support |should_replace_current_entry|.
  NOTIMPLEMENTED();  // for |should_replace_current_entry
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  GetFrameTreeServer()->RequestNavigate(
      web_view::NAVIGATION_TARGET_TYPE_EXISTING_FRAME, id_, url_request.Pass());
}

void HTMLFrame::reload(bool ignore_cache, bool is_client_redirect) {
  NOTIMPLEMENTED();
}

void HTMLFrame::forwardInputEvent(const blink::WebInputEvent* event) {
  NOTIMPLEMENTED();
}

}  // namespace mojo
