// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/paint/DisplayItemListPaintTest.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

using TableCellPainterTest = DisplayItemListPaintTest;

// TODO(wangxianzhu): Create a version for slimming paint v2 when it supports interest rect
TEST_F(TableCellPainterTest, TableCellBackgroundInterestRect)
{
    setBodyInnerHTML(
        "<style>"
        "  td { width: 200px; height: 200px; border: none; }"
        "  tr { background-color: blue; }"
        "  table { border: none; border-spacing: 0; border-collapse: collapse; }"
        "</style>"
        "<table>"
        "  <tr><td id='cell1'></td></tr>"
        "  <tr><td id='cell2'></td></tr>"
        "</table>");

    LayoutView& layoutView = *document().layoutView();
    DeprecatedPaintLayer& rootLayer = *layoutView.layer();
    LayoutObject& cell1 = *document().getElementById("cell1")->layoutObject();
    LayoutObject& cell2 = *document().getElementById("cell2")->layoutObject();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 200, 200), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(cell1, DisplayItem::TableCellBackgroundFromContainers));

    DeprecatedPaintLayerPaintingInfo paintingInfo1(&rootLayer, LayoutRect(0, 300, 200, 200), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo1, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 2,
        TestDisplayItem(layoutView, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(cell2, DisplayItem::TableCellBackgroundFromContainers));
}

} // namespace blink
