// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkPictureBuilder_h
#define SkPictureBuilder_h

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "wtf/OwnPtr.h"

namespace blink {

// When slimming paint ships we can remove this SkPicture abstraction and
// rely on DisplayItemList here.
class SkPictureBuilder {
    WTF_MAKE_NONCOPYABLE(SkPictureBuilder);
    STACK_ALLOCATED();
public:
    SkPictureBuilder(const FloatRect& bounds, SkMetaData* metaData = 0, GraphicsContext* containingContext = 0)
        : m_bounds(bounds)
    {
        GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
        if (containingContext && containingContext->contextDisabled())
            disabledMode = GraphicsContext::FullyDisabled;

        m_displayItemList = DisplayItemList::create();
        m_context = adoptPtr(new GraphicsContext(m_displayItemList.get(), disabledMode, metaData));

        if (containingContext) {
            m_context->setDeviceScaleFactor(containingContext->deviceScaleFactor());
            m_context->setPrinting(containingContext->printing());
        }
    }

    GraphicsContext& context() { return *m_context; }

    PassRefPtr<const SkPicture> endRecording()
    {
        m_context->beginRecording(m_bounds);
        m_displayItemList->commitNewDisplayItems();
        m_displayItemList->paintArtifact().replay(*m_context);
        return m_context->endRecording();
    }

private:
    OwnPtr<DisplayItemList> m_displayItemList;
    OwnPtr<GraphicsContext> m_context;
    FloatRect m_bounds;
};

} // namespace blink

#endif // SkPictureBuilder_h
