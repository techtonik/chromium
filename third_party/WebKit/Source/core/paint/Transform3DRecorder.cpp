// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/Transform3DRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"

namespace blink {

Transform3DRecorder::Transform3DRecorder(
    GraphicsContext& context,
    const DisplayItemClientWrapper& client,
    DisplayItem::Type type,
    const TransformationMatrix& transform,
    const FloatPoint3D& transformOrigin)
    : m_context(context)
    , m_client(client)
    , m_type(type)
{
    ASSERT(DisplayItem::isTransform3DType(type));
    m_skipRecordingForIdentityTransform = transform.isIdentity();

    if (m_skipRecordingForIdentityTransform)
        return;

    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->createAndAppend<BeginTransform3DDisplayItem>(m_client, m_type, transform, transformOrigin);
}

Transform3DRecorder::~Transform3DRecorder()
{
    if (m_skipRecordingForIdentityTransform)
        return;

    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->endItem<EndTransform3DDisplayItem>(m_client, DisplayItem::transform3DTypeToEndTransform3DType(m_type));
}

} // namespace blink
