/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef PaintLayerScrollableArea_h
#define PaintLayerScrollableArea_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintLayerFragment.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

enum ResizerHitTestType {
    ResizerForPointer,
    ResizerForTouch
};

class PlatformEvent;
class LayoutBox;
class PaintLayer;
class LayoutScrollbarPart;

class CORE_EXPORT PaintLayerScrollableArea final : public NoBaseWillBeGarbageCollectedFinalized<PaintLayerScrollableArea>, public ScrollableArea {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(PaintLayerScrollableArea);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PaintLayerScrollableArea);
    friend class Internals;

private:
    class ScrollbarManager {
        DISALLOW_ALLOCATION();

        // Helper class to manage the life cycle of Scrollbar objects.  Some layout containers
        // (e.g., flexbox, table) run multi-pass layout on their children, applying different
        // constraints.  If a child has overflow:auto, it may gain and lose scrollbars multiple
        // times during multi-pass layout, causing pointless allocation/deallocation thrashing,
        // and potentially leading to other problems (crbug.com/528940).

        // ScrollbarManager allows a ScrollableArea to delay the destruction of a scrollbar that
        // is no longer needed, until the end of multi-pass layout.  If the scrollbar is then
        // re-added before multi-pass layout finishes, the previously "deleted" scrollbar will
        // be restored, rather than constructing a new one.
    public:
        ScrollbarManager(PaintLayerScrollableArea&);

        void dispose();

        // When canDetachScrollbars is true, calls to setHas*Scrollbar(false) will NOT destroy
        // an existing scrollbar, but instead detach it without destroying it.  If, subsequently,
        // setHas*Scrollbar(true) is called, the existing scrollbar will be reattached.  When
        // setCanDetachScrollbars(false) is called, any detached scrollbars will be destructed.
        bool canDetachScrollbars() const { return m_canDetachScrollbars; }
        void setCanDetachScrollbars(bool);

        Scrollbar* horizontalScrollbar() const { return m_hBarIsAttached ? m_hBar.get(): nullptr; }
        Scrollbar* verticalScrollbar() const { return m_vBarIsAttached ? m_vBar.get() : nullptr; }
        bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
        bool hasVerticalScrollbar() const { return verticalScrollbar(); }

        void setHasHorizontalScrollbar(bool hasScrollbar);
        void setHasVerticalScrollbar(bool hasScrollbar);

        DECLARE_TRACE();

    private:
        PassRefPtrWillBeRawPtr<Scrollbar> createScrollbar(ScrollbarOrientation);
        void destroyScrollbar(ScrollbarOrientation, bool invalidate = false);

    private:
        RawPtrWillBeMember<PaintLayerScrollableArea> m_scrollableArea;
        RefPtrWillBeMember<Scrollbar> m_hBar;
        RefPtrWillBeMember<Scrollbar> m_vBar;
        unsigned m_canDetachScrollbars: 1;
        unsigned m_hBarIsAttached: 1;
        unsigned m_vBarIsAttached: 1;
    };

public:
    // FIXME: We should pass in the LayoutBox but this opens a window
    // for crashers during PaintLayer setup (see crbug.com/368062).
    static PassOwnPtrWillBeRawPtr<PaintLayerScrollableArea> create(PaintLayer& layer)
    {
        return adoptPtrWillBeNoop(new PaintLayerScrollableArea(layer));
    }

    ~PaintLayerScrollableArea() override;
    void dispose();

    bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
    bool hasVerticalScrollbar() const { return verticalScrollbar(); }

    Scrollbar* horizontalScrollbar() const override { return m_scrollbarManager.horizontalScrollbar(); }
    Scrollbar* verticalScrollbar() const override { return m_scrollbarManager.verticalScrollbar(); }

    HostWindow* hostWindow() const override;

    GraphicsLayer* layerForScrolling() const override;
    GraphicsLayer* layerForHorizontalScrollbar() const override;
    GraphicsLayer* layerForVerticalScrollbar() const override;
    GraphicsLayer* layerForScrollCorner() const override;
    bool usesCompositedScrolling() const override;
    void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    void invalidateScrollCornerRect(const IntRect&) override;
    bool shouldUseIntegerScrollOffset() const override;
    bool isActive() const override;
    bool isScrollCornerVisible() const override;
    IntRect scrollCornerRect() const override;
    IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const override;
    IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const override;
    IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const override;
    IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const override;
    int scrollSize(ScrollbarOrientation) const override;
    IntPoint scrollPosition() const override;
    DoublePoint scrollPositionDouble() const override;
    IntPoint minimumScrollPosition() const override;
    IntPoint maximumScrollPosition() const override;
    IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    int visibleHeight() const override;
    int visibleWidth() const override;
    IntSize contentsSize() const override;
    IntPoint lastKnownMousePosition() const override;
    bool scrollAnimatorEnabled() const override;
    bool shouldSuspendScrollAnimations() const override;
    bool scrollbarsCanBeActive() const override;
    void scrollbarVisibilityChanged() override;
    IntRect scrollableAreaBoundingBox() const override;
    void registerForAnimation() override;
    void deregisterForAnimation() override;
    bool userInputScrollable(ScrollbarOrientation) const override;
    bool shouldPlaceVerticalScrollbarOnLeft() const override;
    int pageStep(ScrollbarOrientation) const override;
    ScrollBehavior scrollBehaviorStyle() const override;

    double scrollXOffset() const { return m_scrollOffset.width() + scrollOrigin().x(); }
    double scrollYOffset() const { return m_scrollOffset.height() + scrollOrigin().y(); }

    DoubleSize scrollOffset() const { return m_scrollOffset; }

    // FIXME: We shouldn't allow access to m_overflowRect outside this class.
    LayoutRect overflowRect() const { return m_overflowRect; }

    void scrollToPosition(const DoublePoint& scrollPosition, ScrollOffsetClamping = ScrollOffsetUnclamped, ScrollBehavior = ScrollBehaviorInstant, ScrollType = ProgrammaticScroll);

    void scrollToOffset(const DoubleSize& scrollOffset, ScrollOffsetClamping clamp = ScrollOffsetUnclamped, ScrollBehavior scrollBehavior = ScrollBehaviorInstant)
    {
        scrollToPosition(-scrollOrigin() + scrollOffset, clamp, scrollBehavior);
    }

    void scrollToXOffset(double x, ScrollOffsetClamping clamp = ScrollOffsetUnclamped, ScrollBehavior scrollBehavior = ScrollBehaviorInstant)
    {
        scrollToOffset(DoubleSize(x, scrollYOffset()), clamp, scrollBehavior);
    }

    void scrollToYOffset(double y, ScrollOffsetClamping clamp = ScrollOffsetUnclamped, ScrollBehavior scrollBehavior = ScrollBehaviorInstant)
    {
        scrollToOffset(DoubleSize(scrollXOffset(), y), clamp, scrollBehavior);
    }

    void setScrollPosition(const DoublePoint& position, ScrollType scrollType, ScrollBehavior scrollBehavior = ScrollBehaviorInstant) override
    {
        scrollToOffset(toDoubleSize(position), ScrollOffsetClamped, scrollBehavior);
    }

    void updateScrollDimensions(DoubleSize& scrollOffset, bool& autoHorizontalScrollBarChanged, bool& autoVerticalScrollBarChanged);
    void finalizeScrollDimensions(const DoubleSize& originalScrollOffset, bool autoHorizontalScrollBarChanged, bool autoVerticalScrollBarChanged);

    void updateAfterLayout();
    void updateAfterStyleChange(const ComputedStyle*);
    void updateAfterOverflowRecalc();

    bool updateAfterCompositingChange() override;

    bool hasScrollbar() const { return hasHorizontalScrollbar() || hasVerticalScrollbar(); }

    LayoutScrollbarPart* scrollCorner() const { return m_scrollCorner; }

    void resize(const PlatformEvent&, const LayoutSize&);
    IntSize offsetFromResizeCorner(const IntPoint& absolutePoint) const;

    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool inResizeMode) { m_inResizeMode = inResizeMode; }

    IntRect touchResizerCornerRect(const IntRect& bounds) const
    {
        return resizerCornerRect(bounds, ResizerForTouch);
    }

    LayoutUnit scrollWidth() const;
    LayoutUnit scrollHeight() const;
    int pixelSnappedScrollWidth() const;
    int pixelSnappedScrollHeight() const;

    int verticalScrollbarWidth(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;
    int horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;

    DoubleSize adjustedScrollOffset() const { return DoubleSize(scrollXOffset(), scrollYOffset()); }

    void positionOverflowControls();

    // isPointInResizeControl() is used for testing if a pointer/touch position is in the resize control
    // area.
    bool isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType) const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);

    bool hitTestResizerInFragments(const PaintLayerFragments&, const HitTestLocation&) const;

    LayoutRect scrollIntoView(const LayoutRect&, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType = ProgrammaticScroll) override;

    // Returns true if scrollable area is in the FrameView's collection of scrollable areas. This can
    // only happen if we're scrollable, visible to hit test, and do in fact overflow. This means that
    // 'overflow: hidden' or 'pointer-events: none' layers never get added to the FrameView's collection.
    bool scrollsOverflow() const { return m_scrollsOverflow; }

    // Rectangle encompassing the scroll corner and resizer rect.
    IntRect scrollCornerAndResizerRect() const;

    enum LCDTextMode {
        ConsiderLCDText,
        IgnoreLCDText
    };

    void updateNeedsCompositedScrolling(LCDTextMode = ConsiderLCDText);
    bool needsCompositedScrolling() const { return m_needsCompositedScrolling; }

    // These are used during compositing updates to determine if the overflow
    // controls need to be repositioned in the GraphicsLayer tree.
    void setTopmostScrollChild(PaintLayer*);
    PaintLayer* topmostScrollChild() const { ASSERT(!m_nextTopmostScrollChild); return m_topmostScrollChild; }

    IntRect resizerCornerRect(const IntRect&, ResizerHitTestType) const;

    LayoutBox& box() const;
    PaintLayer* layer() const;

    LayoutScrollbarPart* resizer() { return m_resizer; }

    const IntPoint& cachedOverlayScrollbarOffset() { return m_cachedOverlayScrollbarOffset; }
    void setCachedOverlayScrollbarOffset(const IntPoint& offset) { m_cachedOverlayScrollbarOffset = offset; }

    IntRect rectForHorizontalScrollbar(const IntRect& borderBoxRect) const;
    IntRect rectForVerticalScrollbar(const IntRect& borderBoxRect) const;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit PaintLayerScrollableArea(PaintLayer&);

    bool hasHorizontalOverflow() const;
    bool hasVerticalOverflow() const;
    bool hasScrollableHorizontalOverflow() const;
    bool hasScrollableVerticalOverflow() const;
    bool visualViewportSuppliesScrollbars() const;

    bool needsScrollbarReconstruction() const;

    void computeScrollDimensions();

    void setScrollOffset(const IntPoint&, ScrollType) override;
    void setScrollOffset(const DoublePoint&, ScrollType) override;

    LayoutUnit verticalScrollbarStart(int minX, int maxX) const;
    LayoutUnit horizontalScrollbarStart(int minX) const;
    IntSize scrollbarOffset(const Scrollbar*) const;

    void setHasHorizontalScrollbar(bool hasScrollbar);
    void setHasVerticalScrollbar(bool hasScrollbar);

    void updateScrollCornerStyle();

    // See comments on isPointInResizeControl.
    void updateResizerAreaSet();
    void updateResizerStyle();


    void updateScrollableAreaSet(bool hasOverflow);

    void updateCompositingLayersAfterScroll();

    PaintLayer& m_layer;

    // Keeps track of whether the layer is currently resizing, so events can cause resizing to start and stop.
    unsigned m_inResizeMode : 1;
    unsigned m_scrollsOverflow : 1;

    unsigned m_inOverflowRelayout : 1;

    PaintLayer* m_nextTopmostScrollChild;
    PaintLayer* m_topmostScrollChild;

    // FIXME: once cc can handle composited scrolling with clip paths, we will
    // no longer need this bit.
    unsigned m_needsCompositedScrolling : 1;

    // The width/height of our scrolled area.
    LayoutRect m_overflowRect;

    // ScrollbarManager holds the Scrollbar instances.
    ScrollbarManager m_scrollbarManager;

    // This is the (scroll) offset from scrollOrigin().
    DoubleSize m_scrollOffset;

    IntPoint m_cachedOverlayScrollbarOffset;

    // LayoutObject to hold our custom scroll corner.
    LayoutScrollbarPart* m_scrollCorner;

    // LayoutObject to hold our custom resizer.
    LayoutScrollbarPart* m_resizer;

#if ENABLE(ASSERT)
    bool m_hasBeenDisposed;
#endif
};

} // namespace blink

#endif // LayerScrollableArea_h
