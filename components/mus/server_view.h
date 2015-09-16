// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SERVER_VIEW_H_
#define COMPONENTS_MUS_SERVER_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "base/observer_list.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/ids.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/text_input_state.h"

namespace view_manager {

class ServerViewDelegate;
class ServerViewObserver;

// Server side representation of a view. Delegate is informed of interesting
// events.
//
// It is assumed that all functions that mutate the tree have validated the
// mutation is possible before hand. For example, Reorder() assumes the supplied
// view is a child and not already in position.
//
// ServerViews do not own their children. If you delete a view that has children
// the children are implicitly removed. Similarly if a view has a parent and the
// view is deleted the deleted view is implicitly removed from the parent.
class ServerView : public mojo::Surface, public cc::SurfaceFactoryClient {
 public:
  ServerView(ServerViewDelegate* delegate, const ViewId& id);
  ~ServerView() override;

  void AddObserver(ServerViewObserver* observer);
  void RemoveObserver(ServerViewObserver* observer);

  // Binds the provided |request| to |this| object. If an interface is already
  // bound to this ServerView then the old connection is closed first.
  void Bind(mojo::InterfaceRequest<Surface> request,
            mojo::SurfaceClientPtr client);

  const ViewId& id() const { return id_; }

  void Add(ServerView* child);
  void Remove(ServerView* child);
  void Reorder(ServerView* child,
               ServerView* relative,
               mojo::OrderDirection direction);

  const gfx::Rect& bounds() const { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  const ServerView* parent() const { return parent_; }
  ServerView* parent() { return parent_; }

  const ServerView* GetRoot() const;
  ServerView* GetRoot() {
    return const_cast<ServerView*>(
        const_cast<const ServerView*>(this)->GetRoot());
  }

  std::vector<const ServerView*> GetChildren() const;
  std::vector<ServerView*> GetChildren();

  // Returns true if this contains |view| or is |view|.
  bool Contains(const ServerView* view) const;

  // Returns true if the window is visible. This does not consider visibility
  // of any ancestors.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  float opacity() const { return opacity_; }
  void SetOpacity(float value);

  const gfx::Transform& transform() const { return transform_; }
  void SetTransform(const gfx::Transform& transform);

  const std::map<std::string, std::vector<uint8_t>>& properties() const {
    return properties_;
  }
  void SetProperty(const std::string& name, const std::vector<uint8_t>* value);

  void SetTextInputState(const ui::TextInputState& state);
  const ui::TextInputState& text_input_state() const {
    return text_input_state_;
  }

  // Returns true if this view is attached to a root and all ancestors are
  // visible.
  bool IsDrawn() const;

  void SetSurfaceId(cc::SurfaceId surface_id);
  const cc::SurfaceId& surface_id() const { return surface_id_; }

  const gfx::Size& last_submitted_frame_size() {
    return last_submitted_frame_size_;
  }

  // mojo::Surface:
  void SubmitCompositorFrame(
      mojo::CompositorFramePtr frame,
      const SubmitCompositorFrameCallback& callback) override;

#if !defined(NDEBUG)
  std::string GetDebugWindowHierarchy() const;
  void BuildDebugInfo(const std::string& depth, std::string* result) const;
#endif

 private:
  typedef std::vector<ServerView*> Views;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  // Implementation of removing a view. Doesn't send any notification.
  void RemoveImpl(ServerView* view);

  ServerViewDelegate* delegate_;
  const ViewId id_;
  ServerView* parent_;
  Views children_;
  bool visible_;
  gfx::Rect bounds_;
  cc::SurfaceId surface_id_;
  scoped_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  scoped_ptr<cc::SurfaceFactory> surface_factory_;
  float opacity_;
  gfx::Transform transform_;
  ui::TextInputState text_input_state_;
  gfx::Size last_submitted_frame_size_;

  std::map<std::string, std::vector<uint8_t>> properties_;

  base::ObserverList<ServerViewObserver> observers_;
  mojo::SurfaceClientPtr client_;
  mojo::Binding<Surface> binding_;

  DISALLOW_COPY_AND_ASSIGN(ServerView);
};

}  // namespace view_manager

#endif  // COMPONENTS_MUS_SERVER_VIEW_H_
