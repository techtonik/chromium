// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/view.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "components/mus/public/cpp/lib/view_private.h"
#include "components/mus/public/cpp/lib/view_tree_client_impl.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_surface.h"
#include "components/mus/public/cpp/view_tracker.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

namespace mus {

namespace {

void NotifyViewTreeChangeAtReceiver(
    View* receiver,
    const ViewObserver::TreeChangeParams& params,
    bool change_applied) {
  ViewObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  if (change_applied) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(receiver).observers(),
                      OnTreeChanged(local_params));
  } else {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(receiver).observers(),
                      OnTreeChanging(local_params));
  }
}

void NotifyViewTreeChangeUp(View* start_at,
                            const ViewObserver::TreeChangeParams& params,
                            bool change_applied) {
  for (View* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params, change_applied);
}

void NotifyViewTreeChangeDown(View* start_at,
                              const ViewObserver::TreeChangeParams& params,
                              bool change_applied) {
  NotifyViewTreeChangeAtReceiver(start_at, params, change_applied);
  View::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params, change_applied);
}

void NotifyViewTreeChange(const ViewObserver::TreeChangeParams& params,
                          bool change_applied) {
  NotifyViewTreeChangeDown(params.target, params, change_applied);
  if (params.old_parent)
    NotifyViewTreeChangeUp(params.old_parent, params, change_applied);
  if (params.new_parent)
    NotifyViewTreeChangeUp(params.new_parent, params, change_applied);
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(View* target, View* old_parent, View* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_, false);
  }
  ~ScopedTreeNotifier() { NotifyViewTreeChange(params_, true); }

 private:
  ViewObserver::TreeChangeParams params_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(View* child, View::Children* children) {
  View::Children::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    ViewPrivate(child).ClearParent();
  }
}

class ScopedOrderChangedNotifier {
 public:
  ScopedOrderChangedNotifier(View* view,
                             View* relative_view,
                             mojo::OrderDirection direction)
      : view_(view), relative_view_(relative_view), direction_(direction) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view_).observers(),
                      OnViewReordering(view_, relative_view_, direction_));
  }
  ~ScopedOrderChangedNotifier() {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view_).observers(),
                      OnViewReordered(view_, relative_view_, direction_));
  }

 private:
  View* view_;
  View* relative_view_;
  mojo::OrderDirection direction_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedOrderChangedNotifier);
};

// Returns true if the order actually changed.
bool ReorderImpl(View::Children* children,
                 View* view,
                 View* relative,
                 mojo::OrderDirection direction) {
  DCHECK(relative);
  DCHECK_NE(view, relative);
  DCHECK_EQ(view->parent(), relative->parent());

  const size_t child_i =
      std::find(children->begin(), children->end(), view) - children->begin();
  const size_t target_i =
      std::find(children->begin(), children->end(), relative) -
      children->begin();
  if ((direction == mojo::ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == mojo::ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  ScopedOrderChangedNotifier notifier(view, relative, direction);

  const size_t dest_i = direction == mojo::ORDER_DIRECTION_ABOVE
                            ? (child_i < target_i ? target_i : target_i + 1)
                            : (child_i < target_i ? target_i - 1 : target_i);
  children->erase(children->begin() + child_i);
  children->insert(children->begin() + dest_i, view);

  return true;
}

class ScopedSetBoundsNotifier {
 public:
  ScopedSetBoundsNotifier(View* view,
                          const mojo::Rect& old_bounds,
                          const mojo::Rect& new_bounds)
      : view_(view), old_bounds_(old_bounds), new_bounds_(new_bounds) {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view_).observers(),
                      OnViewBoundsChanging(view_, old_bounds_, new_bounds_));
  }
  ~ScopedSetBoundsNotifier() {
    FOR_EACH_OBSERVER(ViewObserver, *ViewPrivate(view_).observers(),
                      OnViewBoundsChanged(view_, old_bounds_, new_bounds_));
  }

 private:
  View* view_;
  const mojo::Rect old_bounds_;
  const mojo::Rect new_bounds_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedSetBoundsNotifier);
};

// Some operations are only permitted in the connection that created the view.
bool OwnsView(ViewTreeConnection* connection, View* view) {
  return !connection ||
         static_cast<ViewTreeClientImpl*>(connection)->OwnsView(view->id());
}

void EmptyEmbedCallback(bool result, ConnectionSpecificId connection_id) {}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// View, public:

void View::Destroy() {
  if (!OwnsView(connection_, this))
    return;

  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->DestroyView(id_);
  while (!children_.empty()) {
    View* child = children_.front();
    if (!OwnsView(connection_, child)) {
      ViewPrivate(child).ClearParent();
      children_.erase(children_.begin());
    } else {
      child->Destroy();
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    }
  }
  LocalDestroy();
}

void View::SetBounds(const mojo::Rect& bounds) {
  if (!OwnsView(connection_, this))
    return;

  if (bounds_.Equals(bounds))
    return;

  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->SetBounds(id_, bounds);
  LocalSetBounds(bounds_, bounds);
}

void View::SetVisible(bool value) {
  if (visible_ == value)
    return;

  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->SetVisible(id_, value);
  LocalSetVisible(value);
}

scoped_ptr<ViewSurface> View::RequestSurface() {
  mojo::SurfacePtr surface;
  mojo::SurfaceClientPtr client;
  mojo::InterfaceRequest<mojo::SurfaceClient> client_request =
      GetProxy(&client);
  static_cast<ViewTreeClientImpl*>(connection_)
      ->RequestSurface(id_, GetProxy(&surface), client.Pass());
  return make_scoped_ptr(
      new ViewSurface(surface.PassInterface(), client_request.Pass()));
}

void View::SetSharedProperty(const std::string& name,
                             const std::vector<uint8_t>* value) {
  std::vector<uint8_t> old_value;
  std::vector<uint8_t>* old_value_ptr = nullptr;
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    old_value = it->second;
    old_value_ptr = &old_value;

    if (value && old_value == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is NULL, so there's
    // no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }

  // TODO: add test coverage of this (450303).
  if (connection_) {
    mojo::Array<uint8_t> transport_value;
    if (value) {
      transport_value.resize(value->size());
      if (value->size())
        memcpy(&transport_value.front(), &(value->front()), value->size());
    }
    static_cast<ViewTreeClientImpl*>(connection_)
        ->SetProperty(id_, name, transport_value.Pass());
  }

  FOR_EACH_OBSERVER(
      ViewObserver, observers_,
      OnViewSharedPropertyChanged(this, name, old_value_ptr, value));
}

bool View::IsDrawn() const {
  if (!visible_)
    return false;
  return parent_ ? parent_->IsDrawn() : drawn_;
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

const View* View::GetRoot() const {
  const View* root = this;
  for (const View* parent = this; parent; parent = parent->parent())
    root = parent;
  return root;
}

void View::AddChild(View* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (connection_)
    CHECK_EQ(child->connection(), connection_);
  LocalAddChild(child);
  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->AddChild(child->id(), id_);
}

void View::RemoveChild(View* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (connection_)
    CHECK_EQ(child->connection(), connection_);
  LocalRemoveChild(child);
  if (connection_) {
    static_cast<ViewTreeClientImpl*>(connection_)
        ->RemoveChild(child->id(), id_);
  }
}

void View::MoveToFront() {
  if (!parent_ || parent_->children_.back() == this)
    return;
  Reorder(parent_->children_.back(), mojo::ORDER_DIRECTION_ABOVE);
}

void View::MoveToBack() {
  if (!parent_ || parent_->children_.front() == this)
    return;
  Reorder(parent_->children_.front(), mojo::ORDER_DIRECTION_BELOW);
}

void View::Reorder(View* relative, mojo::OrderDirection direction) {
  if (!LocalReorder(relative, direction))
    return;
  if (connection_) {
    static_cast<ViewTreeClientImpl*>(connection_)
        ->Reorder(id_, relative->id(), direction);
  }
}

bool View::Contains(View* child) const {
  if (!child)
    return false;
  if (child == this)
    return true;
  if (connection_)
    CHECK_EQ(child->connection(), connection_);
  for (View* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

View* View::GetChildById(Id id) {
  if (id == id_)
    return this;
  // TODO(beng): this could be improved depending on how we decide to own views.
  Children::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    View* view = (*it)->GetChildById(id);
    if (view)
      return view;
  }
  return NULL;
}

void View::SetTextInputState(mojo::TextInputStatePtr state) {
  if (connection_) {
    static_cast<ViewTreeClientImpl*>(connection_)
        ->SetViewTextInputState(id_, state.Pass());
  }
}

void View::SetImeVisibility(bool visible, mojo::TextInputStatePtr state) {
  // SetImeVisibility() shouldn't be used if the view is not editable.
  DCHECK(state.is_null() || state->type != mojo::TEXT_INPUT_TYPE_NONE);
  if (connection_) {
    static_cast<ViewTreeClientImpl*>(connection_)
        ->SetImeVisibility(id_, visible, state.Pass());
  }
}

void View::SetFocus() {
  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->SetFocus(id_);
}

bool View::HasFocus() const {
  return connection_ && connection_->GetFocusedView() == this;
}

void View::Embed(mojo::ViewTreeClientPtr client) {
  Embed(client.Pass(), mojo::ViewTree::ACCESS_POLICY_DEFAULT,
        base::Bind(&EmptyEmbedCallback));
}

void View::Embed(mojo::ViewTreeClientPtr client,
                 uint32_t policy_bitmask,
                 const EmbedCallback& callback) {
  if (PrepareForEmbed()) {
    static_cast<ViewTreeClientImpl*>(connection_)
        ->Embed(id_, client.Pass(), policy_bitmask, callback);
  } else {
    callback.Run(false, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

namespace {

mojo::ViewportMetricsPtr CreateEmptyViewportMetrics() {
  mojo::ViewportMetricsPtr metrics = mojo::ViewportMetrics::New();
  metrics->size_in_pixels = mojo::Size::New();
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return metrics.Pass();
}

}  // namespace

View::View()
    : connection_(NULL),
      id_(static_cast<Id>(-1)),
      parent_(NULL),
      viewport_metrics_(CreateEmptyViewportMetrics()),
      visible_(true),
      drawn_(false) {}

View::~View() {
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroying(this));
  if (parent_)
    parent_->LocalRemoveChild(this);

  // We may still have children. This can happen if the embedder destroys the
  // root while we're still alive.
  while (!children_.empty()) {
    View* child = children_.front();
    LocalRemoveChild(child);
    DCHECK(children_.empty() || children_.front() != child);
  }

  // TODO(beng): It'd be better to do this via a destruction observer in the
  //             ViewTreeClientImpl.
  if (connection_)
    static_cast<ViewTreeClientImpl*>(connection_)->RemoveView(id_);

  // Clear properties.
  for (auto& pair : prop_map_) {
    if (pair.second.deallocator)
      (*pair.second.deallocator)(pair.second.value);
  }
  prop_map_.clear();

  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroyed(this));

  if (connection_ && connection_->GetRoot() == this)
    static_cast<ViewTreeClientImpl*>(connection_)->OnRootDestroyed(this);
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

View::View(ViewTreeConnection* connection, Id id)
    : connection_(connection),
      id_(id),
      parent_(nullptr),
      viewport_metrics_(CreateEmptyViewportMetrics()),
      visible_(false),
      drawn_(false) {}

int64 View::SetLocalPropertyInternal(const void* key,
                                     const char* name,
                                     PropertyDeallocator deallocator,
                                     int64 value,
                                     int64 default_value) {
  int64 old = GetLocalPropertyInternal(key, default_value);
  if (value == default_value) {
    prop_map_.erase(key);
  } else {
    Value prop_value;
    prop_value.name = name;
    prop_value.value = value;
    prop_value.deallocator = deallocator;
    prop_map_[key] = prop_value;
  }
  FOR_EACH_OBSERVER(ViewObserver, observers_,
                    OnViewLocalPropertyChanged(this, key, old));
  return old;
}

int64 View::GetLocalPropertyInternal(const void* key,
                                     int64 default_value) const {
  std::map<const void*, Value>::const_iterator iter = prop_map_.find(key);
  if (iter == prop_map_.end())
    return default_value;
  return iter->second.value;
}

void View::LocalDestroy() {
  delete this;
}

void View::LocalAddChild(View* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
}

void View::LocalRemoveChild(View* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier notifier(child, this, NULL);
  RemoveChildImpl(child, &children_);
}

bool View::LocalReorder(View* relative, mojo::OrderDirection direction) {
  return ReorderImpl(&parent_->children_, this, relative, direction);
}

void View::LocalSetBounds(const mojo::Rect& old_bounds,
                          const mojo::Rect& new_bounds) {
  DCHECK(old_bounds.x == bounds_.x);
  DCHECK(old_bounds.y == bounds_.y);
  DCHECK(old_bounds.width == bounds_.width);
  DCHECK(old_bounds.height == bounds_.height);
  ScopedSetBoundsNotifier notifier(this, old_bounds, new_bounds);
  bounds_ = new_bounds;
}

void View::LocalSetViewportMetrics(const mojo::ViewportMetrics& old_metrics,
                                   const mojo::ViewportMetrics& new_metrics) {
  // TODO(eseidel): We could check old_metrics against viewport_metrics_.
  viewport_metrics_ = new_metrics.Clone();
  FOR_EACH_OBSERVER(
      ViewObserver, observers_,
      OnViewViewportMetricsChanged(this, old_metrics, new_metrics));
}

void View::LocalSetDrawn(bool value) {
  if (drawn_ == value)
    return;

  // As IsDrawn() is derived from |visible_| and |drawn_|, only send drawn
  // notification is the value of IsDrawn() is really changing.
  if (IsDrawn() == value) {
    drawn_ = value;
    return;
  }
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDrawnChanging(this));
  drawn_ = value;
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDrawnChanged(this));
}

void View::LocalSetVisible(bool visible) {
  if (visible_ == visible)
    return;

  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewVisibilityChanging(this));
  visible_ = visible;
  NotifyViewVisibilityChanged(this);
}

void View::NotifyViewVisibilityChanged(View* target) {
  if (!NotifyViewVisibilityChangedDown(target)) {
    return;  // |this| has been deleted.
  }
  NotifyViewVisibilityChangedUp(target);
}

bool View::NotifyViewVisibilityChangedAtReceiver(View* target) {
  // |this| may be deleted during a call to OnViewVisibilityChanged() on one
  // of the observers. We create an local observer for that. In that case we
  // exit without further access to any members.
  ViewTracker tracker;
  tracker.Add(this);
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewVisibilityChanged(target));
  return tracker.Contains(this);
}

bool View::NotifyViewVisibilityChangedDown(View* target) {
  if (!NotifyViewVisibilityChangedAtReceiver(target))
    return false;  // |this| was deleted.
  std::set<const View*> child_already_processed;
  bool child_destroyed = false;
  do {
    child_destroyed = false;
    for (View::Children::const_iterator it = children_.begin();
         it != children_.end(); ++it) {
      if (!child_already_processed.insert(*it).second)
        continue;
      if (!(*it)->NotifyViewVisibilityChangedDown(target)) {
        // |*it| was deleted, |it| is invalid and |children_| has changed.  We
        // exit the current for-loop and enter a new one.
        child_destroyed = true;
        break;
      }
    }
  } while (child_destroyed);
  return true;
}

void View::NotifyViewVisibilityChangedUp(View* target) {
  // Start with the parent as we already notified |this|
  // in NotifyViewVisibilityChangedDown.
  for (View* view = parent(); view; view = view->parent()) {
    bool ret = view->NotifyViewVisibilityChangedAtReceiver(target);
    DCHECK(ret);
  }
}

bool View::PrepareForEmbed() {
  if (!OwnsView(connection_, this) &&
      !static_cast<ViewTreeClientImpl*>(connection_)->is_embed_root()) {
    return false;
  }

  while (!children_.empty())
    RemoveChild(children_[0]);
  return true;
}

}  // namespace mus
