// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_HOST_FACTORY_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_HOST_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {
class ApplicationImpl;
}

namespace mus {

class ViewTreeDelegate;

// Uses |factory| to create a new |host|, providing the supplied |host_client|
// which may be null. |delegate| must not be null.
void CreateViewTreeHost(mojo::ViewTreeHostFactory* factory,
                        mojo::ViewTreeHostClientPtr host_client,
                        ViewTreeDelegate* delegate,
                        mojo::ViewTreeHostPtr* host);

// Creates a single host with no client by connecting to the view manager
// application. Useful only for tests and trivial UIs.
void CreateSingleViewTreeHost(mojo::ApplicationImpl* app,
                              ViewTreeDelegate* delegate,
                              mojo::ViewTreeHostPtr* host);

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_TREE_HOST_FACTORY_H_
