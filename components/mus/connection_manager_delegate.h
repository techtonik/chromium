// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_CONNECTION_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_CONNECTION_MANAGER_DELEGATE_H_

#include <string>

#include "components/mus/public/cpp/types.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class ViewTree;
}

namespace view_manager {

class ClientConnection;
class ConnectionManager;
struct ViewId;

class ConnectionManagerDelegate {
 public:
  virtual void OnNoMoreRootConnections() = 0;

  // Creates a ClientConnection in response to Embed() calls on the
  // ConnectionManager.
  virtual ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewTree> tree_request,
      mojo::ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ViewId& root_id,
      uint32_t policy_bitmask) = 0;
  virtual ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewTree> tree_request,
      mojo::ConnectionSpecificId creator_id,
      const ViewId& root_id,
      uint32_t policy_bitmask,
      mojo::ViewTreeClientPtr client) = 0;

 protected:
  virtual ~ConnectionManagerDelegate() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_MUS_CONNECTION_MANAGER_DELEGATE_H_
