// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/surface_id.mojom.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace mus {

class ServiceProviderImpl;
class View;
class ViewObserver;
class ViewSurface;
class ViewTreeConnection;

// Defined in view_property.h (which we do not include)
template <typename T>
struct ViewProperty;

// Views are owned by the ViewTreeConnection. See ViewTreeDelegate for details
// on ownership.
//
// TODO(beng): Right now, you'll have to implement a ViewObserver to track
//             destruction and NULL any pointers you have.
//             Investigate some kind of smart pointer or weak pointer for these.
class View {
 public:
  using Children = std::vector<View*>;
  using SharedProperties = std::map<std::string, std::vector<uint8_t>>;
  using EmbedCallback = base::Callback<void(bool, ConnectionSpecificId)>;

  // Destroys this view and all its children. Destruction is allowed for views
  // that were created by this connection. For views from other connections
  // (such as the root) Destroy() does nothing. If the destruction is allowed
  // observers are notified and the View is immediately deleted.
  void Destroy();

  ViewTreeConnection* connection() { return connection_; }

  // Configuration.
  Id id() const { return id_; }

  // Geometric disposition.
  const mojo::Rect& bounds() const { return bounds_; }
  void SetBounds(const mojo::Rect& bounds);

  // Visibility (also see IsDrawn()). When created views are hidden.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  const mojo::ViewportMetrics& viewport_metrics() { return *viewport_metrics_; }

  scoped_ptr<ViewSurface> RequestSurface();

  // Returns the set of string to bag of byte properties. These properties are
  // shared with the view manager.
  const SharedProperties& shared_properties() const { return properties_; }
  // Sets a property. If |data| is null, this property is deleted.
  void SetSharedProperty(const std::string& name,
                         const std::vector<uint8_t>* data);

  // Sets the |value| of the given window |property|. Setting to the default
  // value (e.g., NULL) removes the property. The caller is responsible for the
  // lifetime of any object set as a property on the View.
  //
  // These properties are not visible to the view manager.
  template <typename T>
  void SetLocalProperty(const ViewProperty<T>* property, T value);

  // Returns the value of the given window |property|.  Returns the
  // property-specific default value if the property was not previously set.
  //
  // These properties are only visible in the current process and are not
  // shared with other mojo services.
  template <typename T>
  T GetLocalProperty(const ViewProperty<T>* property) const;

  // Sets the |property| to its default value. Useful for avoiding a cast when
  // setting to NULL.
  //
  // These properties are only visible in the current process and are not
  // shared with other mojo services.
  template <typename T>
  void ClearLocalProperty(const ViewProperty<T>* property);

  // Type of a function to delete a property that this view owns.
  typedef void (*PropertyDeallocator)(int64_t value);

  // A View is drawn if the View and all its ancestors are visible and the
  // View is attached to the root.
  bool IsDrawn() const;

  // Observation.
  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // Tree.
  View* parent() { return parent_; }
  const View* parent() const { return parent_; }
  const Children& children() const { return children_; }
  View* GetRoot() {
    return const_cast<View*>(const_cast<const View*>(this)->GetRoot());
  }
  const View* GetRoot() const;

  void AddChild(View* child);
  void RemoveChild(View* child);

  void Reorder(View* relative, mojo::OrderDirection direction);
  void MoveToFront();
  void MoveToBack();

  bool Contains(View* child) const;

  View* GetChildById(Id id);

  void SetTextInputState(mojo::TextInputStatePtr state);
  void SetImeVisibility(bool visible, mojo::TextInputStatePtr state);

  // Focus.
  void SetFocus();
  bool HasFocus() const;

  // Embedding. See view_tree.mojom for details.
  void Embed(mojo::ViewTreeClientPtr client);

  // NOTE: callback is run synchronously if Embed() is not allowed on this
  // View.
  void Embed(mojo::ViewTreeClientPtr client,
             uint32_t policy_bitmask,
             const EmbedCallback& callback);

 protected:
  // This class is subclassed only by test classes that provide a public ctor.
  View();
  ~View();

 private:
  friend class ViewPrivate;
  friend class ViewTreeClientImpl;

  View(ViewTreeConnection* connection, Id id);

  // Called by the public {Set,Get,Clear}Property functions.
  int64_t SetLocalPropertyInternal(const void* key,
                                   const char* name,
                                   PropertyDeallocator deallocator,
                                   int64_t value,
                                   int64_t default_value);
  int64_t GetLocalPropertyInternal(const void* key,
                                   int64_t default_value) const;

  void LocalDestroy();
  void LocalAddChild(View* child);
  void LocalRemoveChild(View* child);
  // Returns true if the order actually changed.
  bool LocalReorder(View* relative, mojo::OrderDirection direction);
  void LocalSetBounds(const mojo::Rect& old_bounds,
                      const mojo::Rect& new_bounds);
  void LocalSetViewportMetrics(const mojo::ViewportMetrics& old_metrics,
                               const mojo::ViewportMetrics& new_metrics);
  void LocalSetDrawn(bool drawn);
  void LocalSetVisible(bool visible);

  // Methods implementing visibility change notifications. See ViewObserver
  // for more details.
  void NotifyViewVisibilityChanged(View* target);
  // Notifies this view's observers. Returns false if |this| was deleted during
  // the call (by an observer), otherwise true.
  bool NotifyViewVisibilityChangedAtReceiver(View* target);
  // Notifies this view and its child hierarchy. Returns false if |this| was
  // deleted during the call (by an observer), otherwise true.
  bool NotifyViewVisibilityChangedDown(View* target);
  // Notifies this view and its parent hierarchy.
  void NotifyViewVisibilityChangedUp(View* target);

  // Returns true if embed is allowed for this node. If embedding is allowed all
  // the children are removed.
  bool PrepareForEmbed();

  ViewTreeConnection* connection_;
  Id id_;
  View* parent_;
  Children children_;

  base::ObserverList<ViewObserver> observers_;

  mojo::Rect bounds_;
  mojo::ViewportMetricsPtr viewport_metrics_;

  bool visible_;

  SharedProperties properties_;

  // Drawn state is derived from the visible state and the parent's visible
  // state. This field is only used if the view has no parent (eg it's a root).
  bool drawn_;

  // Value struct to keep the name and deallocator for this property.
  // Key cannot be used for this purpose because it can be char* or
  // WindowProperty<>.
  struct Value {
    const char* name;
    int64_t value;
    PropertyDeallocator deallocator;
  };

  std::map<const void*, Value> prop_map_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_H_
