/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef PaintLayerReflectionInfo_h
#define PaintLayerReflectionInfo_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/PaintLayerPainter.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PaintLayer;
class LayoutReplica;

class PaintLayerReflectionInfo {
    WTF_MAKE_FAST_ALLOCATED(PaintLayerReflectionInfo);
    WTF_MAKE_NONCOPYABLE(PaintLayerReflectionInfo);
public:
    explicit PaintLayerReflectionInfo(LayoutBox&);
    void destroy();

    LayoutReplica* reflection() const { return m_reflection; }
    PaintLayer* reflectionLayer() const;

    bool isPaintingInsideReflection() const { return m_isPaintingInsideReflection; }

    void updateAfterStyleChange(const ComputedStyle* oldStyle);

    void paint(GraphicsContext*, const PaintLayerPaintingInfo&, PaintLayerFlags);

private:
    LayoutBox& box() { return *m_box; }
    const LayoutBox& box() const { return *m_box; }

    LayoutBox* m_box;
    LayoutReplica* m_reflection;

    // A state bit tracking if we are painting inside a replica.
    unsigned m_isPaintingInsideReflection : 1;
};

} // namespace blink

#endif // PaintLayerReflectinInfo_h
