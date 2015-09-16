// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/phone_ui/phone_browser_application_delegate.h"

#include "base/command_line.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, public:

PhoneBrowserApplicationDelegate::PhoneBrowserApplicationDelegate()
    : app_(nullptr),
      root_(nullptr),
      content_(nullptr),
      web_view_(this),
      default_url_("http://www.google.com/") {
}

PhoneBrowserApplicationDelegate::~PhoneBrowserApplicationDelegate() {
  if (root_)
    root_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ApplicationDelegate implementation:

void PhoneBrowserApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (const auto& arg : command_line->GetArgs()) {
    GURL url(arg);
    if (url.is_valid()) {
      default_url_ = url.spec();
      break;
    }
  }
  mojo::CreateSingleViewTreeHost(app_, this, &host_);
}

bool PhoneBrowserApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<LaunchHandler>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, LaunchHandler implementation:

void PhoneBrowserApplicationDelegate::LaunchURL(const mojo::String& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url;
  web_view_.web_view()->LoadRequest(request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ViewTreeDelegate implementation:

void PhoneBrowserApplicationDelegate::OnEmbed(mojo::View* root) {
  CHECK(!root_);
  root_ = root;
  content_ = root->connection()->CreateView();
  root->AddChild(content_);
  content_->SetBounds(root->bounds());
  content_->SetVisible(true);
  root->AddObserver(this);

  host_->SetSize(mojo::Size::From(gfx::Size(320, 640)));
  web_view_.Init(app_, content_);
  LaunchURL(default_url_);
}

void PhoneBrowserApplicationDelegate::OnConnectionLost(
    mojo::ViewTreeConnection* connection) {
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate, mojo::ViewObserver implementation:

void PhoneBrowserApplicationDelegate::OnViewBoundsChanged(
    mojo::View* view,
    const mojo::Rect& old_bounds,
    const mojo::Rect& new_bounds) {
  CHECK_EQ(view, root_);
  content_->SetBounds(
      *mojo::Rect::From(gfx::Rect(0, 0, new_bounds.width, new_bounds.height)));
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate,
//     web_view::mojom::WebViewClient implementation:

void PhoneBrowserApplicationDelegate::TopLevelNavigate(
    mojo::URLRequestPtr request) {
  web_view_.web_view()->LoadRequest(request.Pass());
}

void PhoneBrowserApplicationDelegate::LoadingStateChanged(bool is_loading) {
  // ...
}

void PhoneBrowserApplicationDelegate::ProgressChanged(double progress) {
  // ...
}


void PhoneBrowserApplicationDelegate::BackForwardChanged(
    web_view::mojom::ButtonState back_button,
    web_view::mojom::ButtonState forward_button) {
  // ...
}

void PhoneBrowserApplicationDelegate::TitleChanged(const mojo::String& title) {
  // ...
}

////////////////////////////////////////////////////////////////////////////////
// PhoneBrowserApplicationDelegate,
//       mojo::InterfaceFactory<LaunchHandler> implementation:

void PhoneBrowserApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<LaunchHandler> request) {
  launch_handler_bindings_.AddBinding(this, request.Pass());
}

}  // namespace mandoline
