// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FloatClipRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/FloatClipDisplayItem.h"

namespace blink {

FloatClipRecorder::FloatClipRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, PaintPhase paintPhase, const FloatRect& clipRect)
    : m_context(context)
    , m_client(client)
    , m_clipType(DisplayItem::paintPhaseToFloatClipType(paintPhase))
{
    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->createAndAppend<FloatClipDisplayItem>(m_client, m_clipType, clipRect);
}

FloatClipRecorder::~FloatClipRecorder()
{
    DisplayItem::Type endType = DisplayItem::floatClipTypeToEndFloatClipType(m_clipType);
    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->endItem<EndFloatClipDisplayItem>(m_client, endType);
}

} // namespace blink
