// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_tree_host_connection.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/view_tree_host_impl.h"

namespace view_manager {

ViewTreeHostConnection::ViewTreeHostConnection(
    scoped_ptr<ViewTreeHostImpl> host_impl,
    ConnectionManager* manager)
    : host_(host_impl.Pass()),
      tree_(nullptr),
      connection_manager_(manager),
      connection_closed_(false) {
}

ViewTreeHostConnection::~ViewTreeHostConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}


void ViewTreeHostConnection::CloseConnection() {
  // A connection error will trigger the display to close and so we want to make
  // sure we signal the ConnectionManager only once.
  if (connection_closed_)
    return;
  connection_manager()->OnHostConnectionClosed(this);
  connection_closed_ = true;
  delete this;
}

ViewTreeImpl* ViewTreeHostConnection::GetViewTree() {
  return tree_;
}

void ViewTreeHostConnection::OnDisplayInitialized() {
}

void ViewTreeHostConnection::OnDisplayClosed() {
  CloseConnection();
}

ViewTreeHostConnectionImpl::ViewTreeHostConnectionImpl(
    mojo::InterfaceRequest<mojo::ViewTreeHost> request,
    scoped_ptr<ViewTreeHostImpl> host_impl,
    mojo::ViewTreeClientPtr client,
    ConnectionManager* manager)
    : ViewTreeHostConnection(host_impl.Pass(), manager),
      binding_(view_tree_host(), request.Pass()),
      client_(client.Pass()) {
}

ViewTreeHostConnectionImpl::~ViewTreeHostConnectionImpl() {
}

void ViewTreeHostConnectionImpl::OnDisplayInitialized() {
  connection_manager()->AddHost(this);
  set_view_tree(connection_manager()->EmbedAtView(
      kInvalidConnectionId, view_tree_host()->root_view()->id(),
      client_.Pass()));
}

}  // namespace view_manager
