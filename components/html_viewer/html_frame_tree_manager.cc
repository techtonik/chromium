// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame_tree_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/document_resource_waiter.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_viewer_switches.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

namespace html_viewer {
namespace {

// Returns the index of the FrameData with the id of |frame_id| in |index|. On
// success returns true, otherwise false.
bool FindFrameDataIndex(const mojo::Array<mandoline::FrameDataPtr>& frame_data,
                        uint32_t frame_id,
                        size_t* index) {
  for (size_t i = 0; i < frame_data.size(); ++i) {
    if (frame_data[i]->frame_id == frame_id) {
      *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace

// static
HTMLFrameTreeManager::TreeMap* HTMLFrameTreeManager::instances_ = nullptr;

// static
HTMLFrame* HTMLFrameTreeManager::CreateFrameAndAttachToTree(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    mojo::View* view,
    scoped_ptr<DocumentResourceWaiter> resource_waiter,
    HTMLFrameDelegate* delegate) {
  if (!instances_)
    instances_ = new TreeMap;

  mojo::InterfaceRequest<mandoline::FrameTreeClient> frame_tree_client_request;
  mandoline::FrameTreeServerPtr frame_tree_server;
  mojo::Array<mandoline::FrameDataPtr> frame_data;
  resource_waiter->Release(&frame_tree_client_request, &frame_tree_server,
                           &frame_data);
  resource_waiter.reset();

  HTMLFrameTreeManager* frame_tree = nullptr;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOOPIFAlwaysCreateNewFrameTree)) {
    if (instances_->count(frame_data[0]->frame_id))
      frame_tree = (*instances_)[frame_data[0]->frame_id];
  }

  if (!frame_tree) {
    frame_tree = new HTMLFrameTreeManager(global_state);
    frame_tree->Init(delegate, view, frame_data);
    if (frame_data[0]->frame_id == view->id())
      (*instances_)[frame_data[0]->frame_id] = frame_tree;
  } else {
    // We're going to share a frame tree. There are two possibilities:
    // . We already know about the frame, in which case we swap it to local.
    // . We don't know about the frame (most likely because of timing issues),
    //   but we better know about the parent. Create a new frame for it.
    CHECK(view->id() != frame_data[0]->frame_id);
    HTMLFrame* existing_frame = frame_tree->root_->FindFrame(view->id());
    size_t frame_data_index = 0u;
    CHECK(FindFrameDataIndex(frame_data, view->id(), &frame_data_index));
    const mandoline::FrameDataPtr& data = frame_data[frame_data_index];
    if (existing_frame) {
      CHECK(!existing_frame->IsLocal());
      existing_frame->set_delegate(delegate);
      existing_frame->SwapToLocal(view, data->client_properties);
    } else {
      HTMLFrame* parent = frame_tree->root_->FindFrame(data->parent_id);
      CHECK(parent);
      HTMLFrame::CreateParams params(frame_tree, parent, view->id());
      HTMLFrame* frame = new HTMLFrame(params);
      frame->set_delegate(delegate);
      frame->Init(view, data->client_properties);
    }
  }

  HTMLFrame* frame = frame_tree->root_->FindFrame(view->id());
  DCHECK(frame);
  frame->Bind(frame_tree_server.Pass(), frame_tree_client_request.Pass());
  return frame;
}

blink::WebView* HTMLFrameTreeManager::GetWebView() {
  return root_->web_view();
}

void HTMLFrameTreeManager::OnFrameDestroyed(HTMLFrame* frame) {
  if (frame == root_)
    root_ = nullptr;

  if (frame == local_root_)
    local_root_ = nullptr;

  if (!local_root_ || !local_root_->HasLocalDescendant())
    delete this;
}

HTMLFrameTreeManager::HTMLFrameTreeManager(GlobalState* global_state)
    : global_state_(global_state), root_(nullptr), local_root_(nullptr) {}

HTMLFrameTreeManager::~HTMLFrameTreeManager() {
  DCHECK(!root_ || !local_root_);
  RemoveFromInstances();
}

void HTMLFrameTreeManager::Init(
    HTMLFrameDelegate* delegate,
    mojo::View* local_view,
    const mojo::Array<mandoline::FrameDataPtr>& frame_data) {
  root_ = BuildFrameTree(delegate, frame_data, local_view->id(), local_view);
  local_root_ = root_->FindFrame(local_view->id());
  CHECK(local_root_);
  local_root_->UpdateFocus();
}

HTMLFrame* HTMLFrameTreeManager::BuildFrameTree(
    HTMLFrameDelegate* delegate,
    const mojo::Array<mandoline::FrameDataPtr>& frame_data,
    uint32_t local_frame_id,
    mojo::View* local_view) {
  std::vector<HTMLFrame*> parents;
  HTMLFrame* root = nullptr;
  HTMLFrame* last_frame = nullptr;
  for (size_t i = 0; i < frame_data.size(); ++i) {
    if (last_frame && frame_data[i]->parent_id == last_frame->id()) {
      parents.push_back(last_frame);
    } else if (!parents.empty()) {
      while (parents.back()->id() != frame_data[i]->parent_id)
        parents.pop_back();
    }
    HTMLFrame::CreateParams params(this,
                                   !parents.empty() ? parents.back() : nullptr,
                                   frame_data[i]->frame_id);
    HTMLFrame* frame = new HTMLFrame(params);
    if (!last_frame)
      root = frame;
    else
      DCHECK(frame->parent());
    last_frame = frame;

    if (frame_data[i]->frame_id == local_frame_id)
      frame->set_delegate(delegate);

    frame->Init(local_view, frame_data[i]->client_properties);
  }
  return root;
}

void HTMLFrameTreeManager::RemoveFromInstances() {
  for (auto pair : *instances_) {
    if (pair.second == this) {
      instances_->erase(pair.first);
      return;
    }
  }
}

void HTMLFrameTreeManager::ProcessOnFrameAdded(
    HTMLFrame* source,
    mandoline::FrameDataPtr frame_data) {
  if (source != local_root_)
    return;

  HTMLFrame* parent = root_->FindFrame(frame_data->parent_id);
  if (!parent) {
    DVLOG(1) << "Received invalid parent in OnFrameAdded "
             << frame_data->parent_id;
    return;
  }
  if (root_->FindFrame(frame_data->frame_id)) {
    DVLOG(1) << "Child with id already exists in OnFrameAdded "
             << frame_data->frame_id;
    return;
  }

  HTMLFrame::CreateParams params(this, parent, frame_data->frame_id);
  // |parent| takes ownership of |frame|.
  HTMLFrame* frame = new HTMLFrame(params);
  frame->Init(nullptr, frame_data->client_properties);
}

void HTMLFrameTreeManager::ProcessOnFrameRemoved(HTMLFrame* source,
                                                 uint32_t frame_id) {
  if (source != local_root_)
    return;

  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (!frame) {
    DVLOG(1) << "OnFrameRemoved with unknown frame " << frame_id;
    return;
  }

  // We shouldn't see requests to remove the root.
  if (frame == root_) {
    DVLOG(1) << "OnFrameRemoved supplied root; ignoring";
    return;
  }

  // Requests to remove local frames are followed by the View being destroyed.
  // We handle destruction there.
  if (frame->IsLocal())
    return;

  frame->Close();
}

void HTMLFrameTreeManager::ProcessOnFrameClientPropertyChanged(
    HTMLFrame* source,
    uint32_t frame_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  if (source != local_root_)
    return;

  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (frame)
    frame->SetValueFromClientProperty(name, new_data.Pass());
}

}  // namespace mojo
