// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/test_server_view_delegate.h"
#include "components/view_manager/server_view.h"

namespace view_manager {

TestServerViewDelegate::TestServerViewDelegate() : root_view_(nullptr) {
}

TestServerViewDelegate::~TestServerViewDelegate() {
}

scoped_ptr<cc::CompositorFrame>
TestServerViewDelegate::UpdateViewTreeFromCompositorFrame(
    const mojo::CompositorFramePtr& input) {
  return scoped_ptr<cc::CompositorFrame>();
}

surfaces::SurfacesState* TestServerViewDelegate::GetSurfacesState() {
  return nullptr;
}

void TestServerViewDelegate::OnScheduleViewPaint(const ServerView* view) {
}

const ServerView* TestServerViewDelegate::GetRootView(
    const ServerView* view) const {
  return root_view_;
}

}  // namespace view_manager
