// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_OOPIF_H_
#define COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_OOPIF_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "components/view_manager/public/cpp/view_tree_delegate.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
class View;
class ViewTreeConnection;
}

namespace html_viewer {

class AxProviderImpl;
class DocumentResourceWaiter;
class GlobalState;
class HTMLFactory;
class HTMLFrame;
class HTMLFrameTreeManager;
class TestHTMLViewerImpl;
class WebLayerTreeViewImpl;

// A view for a single HTML document.
//
// HTMLDocument is deleted in one of two ways:
// . When the View the HTMLDocument is embedded in is destroyed.
// . Explicitly by way of Destroy().
class HTMLDocumentOOPIF
    : public mojo::ViewTreeDelegate,
      public mojo::ViewObserver,
      public HTMLFrameDelegate,
      public mojo::InterfaceFactory<mojo::AxProvider>,
      public mojo::InterfaceFactory<mandoline::FrameTreeClient>,
      public mojo::InterfaceFactory<TestHTMLViewer>,
      public mojo::InterfaceFactory<devtools_service::DevToolsAgent>,
      public mojo::InterfaceFactory<mojo::ViewTreeClient> {
 public:
  using DeleteCallback = base::Callback<void(HTMLDocumentOOPIF*)>;

  // Load a new HTMLDocument with |response|.
  // |html_document_app| is the application this app was created in, and
  // |connection| the specific connection triggering this new instance.
  // |setup| is used to obtain init type state (such as resources).
  HTMLDocumentOOPIF(mojo::ApplicationImpl* html_document_app,
                    mojo::ApplicationConnection* connection,
                    mojo::URLResponsePtr response,
                    GlobalState* setup,
                    const DeleteCallback& delete_callback,
                    HTMLFactory* factory);

  // Deletes this object.
  void Destroy();

 private:
  friend class DocumentResourceWaiter;  // So it can call LoadIfNecessary().

  // Requests for interfaces before the document is loaded go here. Once
  // loaded the requests are bound and BeforeLoadCache is deleted.
  struct BeforeLoadCache {
    BeforeLoadCache();
    ~BeforeLoadCache();

    std::set<mojo::InterfaceRequest<mojo::AxProvider>*> ax_provider_requests;
    std::set<mojo::InterfaceRequest<TestHTMLViewer>*> test_interface_requests;
  };

  ~HTMLDocumentOOPIF() override;

  void LoadIfNecessary();
  void Load();

  BeforeLoadCache* GetBeforeLoadCache();

  // ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnConnectionLost(mojo::ViewTreeConnection* connection) override;

  // ViewObserver:
  void OnViewViewportMetricsChanged(
      mojo::View* view,
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override;
  void OnViewDestroyed(mojo::View* view) override;

  // HTMLFrameDelegate:
  void OnFrameDidFinishLoad() override;
  mojo::ApplicationImpl* GetApp() override;
  HTMLFactory* GetHTMLFactory() override;
  void OnFrameSwappedToRemote() override;

  // mojo::InterfaceFactory<mojo::AxProvider>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::AxProvider> request) override;

  // mojo::InterfaceFactory<mandoline::FrameTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mandoline::FrameTreeClient> request) override;

  // mojo::InterfaceFactory<TestHTMLViewer>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<TestHTMLViewer> request) override;

  // mojo::InterfaceFactory<devtools_service::DevToolsAgent>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<devtools_service::DevToolsAgent> request) override;

  // mojo::InterfaceFactory<mojo::ViewTreeClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ViewTreeClient> request) override;

  scoped_ptr<mojo::AppRefCount> app_refcount_;
  mojo::ApplicationImpl* html_document_app_;
  mojo::ApplicationConnection* connection_;

  // HTMLDocument owns these pointers; binding requests after document load.
  std::set<AxProviderImpl*> ax_providers_;

  ScopedVector<TestHTMLViewerImpl> test_html_viewers_;

  // Set to true when the local frame has finished loading.
  bool did_finish_local_frame_load_ = false;

  GlobalState* global_state_;

  HTMLFrame* frame_;

  scoped_ptr<DocumentResourceWaiter> resource_waiter_;

  scoped_ptr<BeforeLoadCache> before_load_cache_;

  DeleteCallback delete_callback_;

  HTMLFactory* factory_;

  mojo::View* root_;

  // Cache interface request of DevToolsAgent if |frame_| hasn't been
  // initialized.
  mojo::InterfaceRequest<devtools_service::DevToolsAgent>
      devtools_agent_request_;

  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentOOPIF);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_DOCUMENT_OOPIF_H_
