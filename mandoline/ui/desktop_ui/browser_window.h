// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
#define MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_

#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "ui/views/layout/layout_manager.h"
#include "url/gurl.h"

namespace mojo {
class ApplicationConnection;
class Shell;
class View;
}

namespace mandoline {

class BrowserManager;
class ProgressView;
class ToolbarView;

class BrowserWindow : public mojo::ViewTreeDelegate,
                      public mojo::ViewTreeHostClient,
                      public web_view::mojom::WebViewClient,
                      public ViewEmbedder,
                      public mojo::InterfaceFactory<ViewEmbedder>,
                      public views::LayoutManager {
 public:
  BrowserWindow(mojo::ApplicationImpl* app,
                mojo::ViewTreeHostFactory* host_factory,
                BrowserManager* manager);

  void LoadURL(const GURL& url);
  void Close();

  void ShowOmnibox();
  void GoBack();
  void GoForward();

 private:
  ~BrowserWindow() override;

  float DIPSToPixels(float value) const;

  // Overridden from mojo::ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnConnectionLost(mojo::ViewTreeConnection* connection) override;

  // Overridden from ViewTreeHostClient:
  void OnAccelerator(uint32_t id, mojo::EventPtr event) override;

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigate(mojo::URLRequestPtr request) override;
  void LoadingStateChanged(bool is_loading) override;
  void ProgressChanged(double progress) override;
  void BackForwardChanged(web_view::mojom::ButtonState back_button,
                          web_view::mojom::ButtonState forward_button) override;
  void TitleChanged(const mojo::String& title) override;

  // Overridden from ViewEmbedder:
  void Embed(mojo::URLRequestPtr request) override;

  // Overridden from mojo::InterfaceFactory<ViewEmbedder>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ViewEmbedder> request) override;


  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  void Init(mojo::View* root);
  void EmbedOmnibox();

  mojo::ApplicationImpl* app_;
  scoped_ptr<AuraInit> aura_init_;
  mojo::ViewTreeHostPtr host_;
  mojo::Binding<ViewTreeHostClient> host_client_binding_;
  BrowserManager* manager_;
  ToolbarView* toolbar_view_;
  ProgressView* progress_bar_;
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_view_;

  mojo::WeakBindingSet<ViewEmbedder> view_embedder_bindings_;

  GURL default_url_;
  GURL current_url_;

  web_view::WebView web_view_;

  OmniboxPtr omnibox_;
  scoped_ptr<mojo::ApplicationConnection> omnibox_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
