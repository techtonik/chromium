// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_TRACKER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_TRACKER_H_

#include <stdint.h>
#include <set>

#include "components/mus/public/cpp/view_observer.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"

namespace mus {

class ViewTracker : public ViewObserver {
 public:
  using Views = std::set<View*>;

  ViewTracker();
  ~ViewTracker() override;

  // Returns the set of views being observed.
  const std::set<View*>& views() const { return views_; }

  // Adds |view| to the set of Views being tracked.
  void Add(View* view);

  // Removes |view| from the set of views being tracked.
  void Remove(View* view);

  // Returns true if |view| was previously added and has not been removed or
  // deleted.
  bool Contains(View* view);

  // ViewObserver overrides:
  void OnViewDestroying(View* view) override;

 private:
  Views views_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewTracker);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_TRACKER_H_
