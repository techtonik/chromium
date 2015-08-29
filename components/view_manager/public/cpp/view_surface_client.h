// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_

namespace mojo {

class ViewSurface;

class ViewSurfaceClient {
 public:
  virtual void OnResourcesReturned(
      ViewSurface* surface,
      mojo::Array<mojo::ReturnedResourcePtr> resources) = 0;

 protected:
  ~ViewSurfaceClient() {}
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_
