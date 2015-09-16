// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/public/cpp/web_view.h"

#include "base/bind.h"
#include "components/mus/public/cpp/view.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace web_view {
namespace {

void OnEmbed(bool success, uint16 connection_id) {
  CHECK(success);
}

}  // namespace

WebView::WebView(mojom::WebViewClient* client) : binding_(client) {}
WebView::~WebView() {}

void WebView::Init(mojo::ApplicationImpl* app, mus::View* view) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:web_view";

  mojom::WebViewClientPtr client;
  mojo::InterfaceRequest<mojom::WebViewClient> client_request =
      GetProxy(&client);
  binding_.Bind(client_request.Pass());

  mojom::WebViewFactoryPtr factory;
  app->ConnectToService(request.Pass(), &factory);
  factory->CreateWebView(client.Pass(), GetProxy(&web_view_));

  mojo::ViewTreeClientPtr view_tree_client;
  web_view_->GetViewTreeClient(GetProxy(&view_tree_client));
  view->Embed(view_tree_client.Pass(), mojo::ViewTree::ACCESS_POLICY_EMBED_ROOT,
              base::Bind(&OnEmbed));
}

}  // namespace web_view
