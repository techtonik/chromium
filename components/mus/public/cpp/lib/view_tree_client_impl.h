// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_TREE_CLIENT_IMPL_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_TREE_CLIENT_IMPL_H_

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
class ViewTreeConnection;
class ViewTreeDelegate;

// Manages the connection with the View Manager service.
class ViewTreeClientImpl : public ViewTreeConnection,
                           public mojo::ViewTreeClient {
 public:
  ViewTreeClientImpl(ViewTreeDelegate* delegate,
                     mojo::InterfaceRequest<mojo::ViewTreeClient> request);
  ~ViewTreeClientImpl() override;

  bool connected() const { return tree_; }
  ConnectionSpecificId connection_id() const { return connection_id_; }

  // API exposed to the view implementations that pushes local changes to the
  // service.
  void DestroyView(Id view_id);

  // These methods take TransportIds. For views owned by the current connection,
  // the connection id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root view.
  void AddChild(Id child_id, Id parent_id);
  void RemoveChild(Id child_id, Id parent_id);

  void Reorder(Id view_id, Id relative_view_id, mojo::OrderDirection direction);

  // Returns true if the specified view was created by this connection.
  bool OwnsView(Id id) const;

  void SetBounds(Id view_id, const mojo::Rect& bounds);
  void SetFocus(Id view_id);
  void SetVisible(Id view_id, bool visible);
  void SetProperty(Id view_id,
                   const std::string& name,
                   const std::vector<uint8_t>& data);
  void SetViewTextInputState(Id view_id, mojo::TextInputStatePtr state);
  void SetImeVisibility(Id view_id,
                        bool visible,
                        mojo::TextInputStatePtr state);

  void Embed(Id view_id,
             mojo::ViewTreeClientPtr client,
             uint32_t policy_bitmask,
             const mojo::ViewTree::EmbedCallback& callback);

  void RequestSurface(Id view_id,
                      mojo::InterfaceRequest<mojo::Surface> surface,
                      mojo::SurfaceClientPtr client);

  void set_change_acked_callback(const mojo::Callback<void(void)>& callback) {
    change_acked_callback_ = callback;
  }
  void ClearChangeAckedCallback() { change_acked_callback_.reset(); }

  // Start/stop tracking views. While tracked, they can be retrieved via
  // ViewTreeConnection::GetViewById.
  void AddView(View* view);
  void RemoveView(Id view_id);

  bool is_embed_root() const { return is_embed_root_; }

  // Called after the root view's observers have been notified of destruction
  // (as the last step of ~View). This ordering ensures that the View Manager
  // is torn down after the root.
  void OnRootDestroyed(View* root);

 private:
  typedef std::map<Id, View*> IdToViewMap;

  Id CreateViewOnServer();

  // Overridden from ViewTreeConnection:
  View* GetRoot() override;
  View* GetViewById(Id id) override;
  View* GetFocusedView() override;
  View* CreateView() override;
  bool IsEmbedRoot() override;
  ConnectionSpecificId GetConnectionId() override;

  // Overridden from ViewTreeClient:
  void OnEmbed(ConnectionSpecificId connection_id,
               mojo::ViewDataPtr root,
               mojo::ViewTreePtr tree,
               Id focused_view_id,
               uint32_t access_policy) override;
  void OnEmbeddedAppDisconnected(Id view_id) override;
  void OnUnembed() override;
  void OnViewBoundsChanged(Id view_id,
                           mojo::RectPtr old_bounds,
                           mojo::RectPtr new_bounds) override;
  void OnViewViewportMetricsChanged(
      mojo::ViewportMetricsPtr old_metrics,
      mojo::ViewportMetricsPtr new_metrics) override;
  void OnViewHierarchyChanged(Id view_id,
                              Id new_parent_id,
                              Id old_parent_id,
                              mojo::Array<mojo::ViewDataPtr> views) override;
  void OnViewReordered(Id view_id,
                       Id relative_view_id,
                       mojo::OrderDirection direction) override;
  void OnViewDeleted(Id view_id) override;
  void OnViewVisibilityChanged(Id view_id, bool visible) override;
  void OnViewDrawnStateChanged(Id view_id, bool drawn) override;
  void OnViewSharedPropertyChanged(Id view_id,
                                   const mojo::String& name,
                                   mojo::Array<uint8_t> new_data) override;
  void OnViewInputEvent(Id view_id,
                        mojo::EventPtr event,
                        const mojo::Callback<void()>& callback) override;
  void OnViewFocused(Id focused_view_id) override;

  void RootDestroyed(View* root);

  void OnActionCompleted(bool success);

  mojo::Callback<void(bool)> ActionCompletedCallback();

  ConnectionSpecificId connection_id_;
  ConnectionSpecificId next_id_;

  mojo::Callback<void(void)> change_acked_callback_;

  ViewTreeDelegate* delegate_;

  View* root_;

  IdToViewMap views_;

  View* capture_view_;
  View* focused_view_;
  View* activated_view_;

  mojo::Binding<ViewTreeClient> binding_;
  mojo::ViewTreePtr tree_;

  bool is_embed_root_;

  bool in_destructor_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewTreeClientImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_TREE_CLIENT_IMPL_H_
