// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipPathRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipPathRecorder::ClipPathRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, const Path& clipPath)
    : m_context(context)
    , m_client(client)
{
    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->createAndAppend<BeginClipPathDisplayItem>(m_client, clipPath);
}

ClipPathRecorder::~ClipPathRecorder()
{
    ASSERT(m_context.displayItemList());
    m_context.displayItemList()->endItem<EndClipPathDisplayItem>(m_client);
}

} // namespace blink
