// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGMaskPainter.h"

#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/paint/CompositingRecorder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

bool SVGMaskPainter::prepareEffect(const LayoutObject& object, GraphicsContext* context)
{
    ASSERT(context);
    ASSERT(m_mask.style());
    ASSERT_WITH_SECURITY_IMPLICATION(!m_mask.needsLayout());

    m_mask.clearInvalidationMask();

    FloatRect paintInvalidationRect = object.paintInvalidationRectInLocalCoordinates();
    if (paintInvalidationRect.isEmpty() || !m_mask.element()->hasChildren())
        return false;

    ASSERT(context->displayItemList());
    context->displayItemList()->createAndAppend<BeginCompositingDisplayItem>(object, SkXfermode::kSrcOver_Mode, 1, &paintInvalidationRect);
    return true;
}

void SVGMaskPainter::finishEffect(const LayoutObject& object, GraphicsContext* context)
{
    ASSERT(context);
    ASSERT(m_mask.style());
    ASSERT_WITH_SECURITY_IMPLICATION(!m_mask.needsLayout());

    FloatRect paintInvalidationRect = object.paintInvalidationRectInLocalCoordinates();
    {
        ColorFilter maskLayerFilter = m_mask.style()->svgStyle().maskType() == MT_LUMINANCE
            ? ColorFilterLuminanceToAlpha : ColorFilterNone;
        CompositingRecorder maskCompositing(*context, object, SkXfermode::kDstIn_Mode, 1, &paintInvalidationRect, maskLayerFilter);
        drawMaskForLayoutObject(context, object, object.objectBoundingBox(), paintInvalidationRect);
    }

    ASSERT(context->displayItemList());
    context->displayItemList()->endItem<EndCompositingDisplayItem>(object);
}

void SVGMaskPainter::drawMaskForLayoutObject(GraphicsContext* context, const LayoutObject& layoutObject, const FloatRect& targetBoundingBox, const FloatRect& targetPaintInvalidationRect)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    RefPtr<const SkPicture> maskContentPicture = m_mask.createContentPicture(contentTransformation, targetBoundingBox, context);

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(*context, layoutObject, DisplayItem::SVGMask, LayoutPoint()))
        return;

    LayoutObjectDrawingRecorder drawingRecorder(*context, layoutObject, DisplayItem::SVGMask, targetPaintInvalidationRect, LayoutPoint());
    context->save();
    context->concatCTM(contentTransformation);
    context->drawPicture(maskContentPicture.get());
    context->restore();
}

}
