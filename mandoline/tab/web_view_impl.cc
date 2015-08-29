// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/web_view_impl.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "components/devtools_service/public/cpp/switches.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "mandoline/tab/frame.h"
#include "mandoline/tab/frame_connection.h"
#include "mandoline/tab/frame_devtools_agent.h"
#include "mandoline/tab/frame_tree.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "url/gurl.h"

// TODO(beng): remove once these classes are in the web_view namespace.
using mandoline::FrameConnection;
using mandoline::FrameTreeClient;
using mandoline::FrameUserData;

namespace web_view {
namespace {

bool EnableRemoteDebugging() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      devtools_service::kRemoteDebuggingPort);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, public:

WebViewImpl::WebViewImpl(mojo::ApplicationImpl* app,
                         mojom::WebViewClientPtr client,
                         mojo::InterfaceRequest<mojom::WebView> request)
    : app_(app),
      client_(client.Pass()),
      binding_(this, request.Pass()),
      content_(nullptr) {
  if (EnableRemoteDebugging())
    devtools_agent_.reset(new FrameDevToolsAgent(app_, this));
}

WebViewImpl::~WebViewImpl() {}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, WebView implementation:

void WebViewImpl::LoadRequest(mojo::URLRequestPtr request) {
  if (!content_) {
    // We haven't been embedded yet, store the request for when we are.
    pending_request_ = request.Pass();
    return;
  }
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::ViewTreeClientPtr view_tree_client;
  frame_connection->Init(app_, request.Pass(), &view_tree_client);

  Frame::ClientPropertyMap client_properties;
  if (devtools_agent_) {
    devtools_service::DevToolsAgentPtr forward_agent;
    frame_connection->application_connection()->ConnectToService(
        &forward_agent);
    devtools_agent_->AttachFrame(forward_agent.Pass(), &client_properties);
  }

  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  frame_tree_.reset(new FrameTree(content_, this, frame_tree_client,
                                  frame_connection.Pass(), client_properties));
  content_->Embed(view_tree_client.Pass());
}

void WebViewImpl::GetViewTreeClient(
    mojo::InterfaceRequest<mojo::ViewTreeClient> view_tree_client) {
  mojo::ViewTreeConnection::Create(this, view_tree_client.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, mojo::ViewTreeDelegate implementation:

void WebViewImpl::OnEmbed(mojo::View* root) {
  root->connection()->SetEmbedRoot();
  root->AddObserver(this);
  content_ = root->connection()->CreateView();
  content_->SetBounds(*mojo::Rect::From(gfx::Rect(0, 0, root->bounds().width,
                                                  root->bounds().height)));
  root->AddChild(content_);
  content_->SetVisible(true);
  content_->AddObserver(this);

  if (!pending_request_.is_null())
    LoadRequest(pending_request_.Pass());
}

void WebViewImpl::OnConnectionLost(mojo::ViewTreeConnection* connection) {
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, mojo::ViewObserver implementation:

void WebViewImpl::OnViewBoundsChanged(mojo::View* view,
                                      const mojo::Rect& old_bounds,
                                      const mojo::Rect& new_bounds) {
  if (view != content_) {
    mojo::Rect rect;
    rect.width = new_bounds.width;
    rect.height = new_bounds.height;
    content_->SetBounds(rect);
  }
}

void WebViewImpl::OnViewDestroyed(mojo::View* view) {
  // |FrameTree| cannot outlive the content view.
  if (view == content_)
    frame_tree_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, mandoline::FrameTreeDelegate implementation:

bool WebViewImpl::CanPostMessageEventToFrame(const Frame* source,
                                             const Frame* target,
                                             HTMLMessageEvent* event) {
  return true;
}

void WebViewImpl::LoadingStateChanged(bool loading) {
  client_->LoadingStateChanged(loading);
}

void WebViewImpl::ProgressChanged(double progress) {
  client_->ProgressChanged(progress);
}

void WebViewImpl::NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) {
  client_->TopLevelNavigate(request.Pass());
}

bool WebViewImpl::CanNavigateFrame(
    Frame* target,
    mojo::URLRequestPtr request,
    FrameTreeClient** frame_tree_client,
    scoped_ptr<FrameUserData>* frame_user_data,
    mojo::ViewTreeClientPtr* view_tree_client) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  frame_connection->Init(app_, request.Pass(), view_tree_client);
  *frame_tree_client = frame_connection->frame_tree_client();
  *frame_user_data = frame_connection.Pass();
  return true;
}

void WebViewImpl::DidStartNavigation(Frame* frame) {}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, FrameDevToolsAgentDelegate implementation:

void WebViewImpl::HandlePageNavigateRequest(const GURL& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url.spec();
  client_->TopLevelNavigate(request.Pass());
}

}  // namespace web_view
