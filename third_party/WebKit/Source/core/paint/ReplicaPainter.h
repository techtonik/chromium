// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReplicaPainter_h
#define ReplicaPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutReplica;

class ReplicaPainter {
    STACK_ALLOCATED();
public:
    ReplicaPainter(LayoutReplica& layoutReplica) : m_layoutReplica(layoutReplica) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    LayoutReplica& m_layoutReplica;
};

} // namespace blink

#endif // ReplicaPainter_h
