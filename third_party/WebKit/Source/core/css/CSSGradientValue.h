/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSGradientValue_h
#define CSSGradientValue_h

#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class FloatPoint;
class Gradient;

enum CSSGradientType {
    CSSDeprecatedLinearGradient,
    CSSDeprecatedRadialGradient,
    CSSPrefixedLinearGradient,
    CSSPrefixedRadialGradient,
    CSSLinearGradient,
    CSSRadialGradient
};
enum CSSGradientRepeat { NonRepeating, Repeating };

// This struct is stack allocated and allocated as part of vectors.
// When allocated on the stack its members are found by conservative
// stack scanning. When allocated as part of Vectors in heap-allocated
// objects its members are visited via the containing object's
// (CSSGradientValue) traceAfterDispatch method.
struct CSSGradientColorStop {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    CSSGradientColorStop() : m_colorIsDerivedFromElement(false) { }
    RefPtrWillBeMember<CSSPrimitiveValue> m_position; // percentage or length
    RefPtrWillBeMember<CSSPrimitiveValue> m_color;
    bool m_colorIsDerivedFromElement;
    bool operator==(const CSSGradientColorStop& other) const
    {
        return compareCSSValuePtr(m_color, other.m_color)
            && compareCSSValuePtr(m_position, other.m_position);
    }
    bool isHint() const
    {
        ASSERT(m_color || m_position);
        return !m_color;
    }

    DECLARE_TRACE();
};

} // namespace blink


// We have to declare the VectorTraits specialization before CSSGradientValue
// declares its inline capacity vector below.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSGradientColorStop);

namespace blink {

class CSSGradientValue : public CSSImageGeneratorValue {
public:
    PassRefPtr<Image> image(const LayoutObject*, const IntSize&);

    void setFirstX(PassRefPtrWillBeRawPtr<CSSValue> val) { m_firstX = val; }
    void setFirstY(PassRefPtrWillBeRawPtr<CSSValue> val) { m_firstY = val; }
    void setSecondX(PassRefPtrWillBeRawPtr<CSSValue> val) { m_secondX = val; }
    void setSecondY(PassRefPtrWillBeRawPtr<CSSValue> val) { m_secondY = val; }

    void addStop(const CSSGradientColorStop& stop) { m_stops.append(stop); }

    unsigned stopCount() const { return m_stops.size(); }

    void appendCSSTextForDeprecatedColorStops(StringBuilder&) const;

    bool isRepeating() const { return m_repeating; }

    CSSGradientType gradientType() const { return m_gradientType; }

    bool isFixedSize() const { return false; }
    IntSize fixedSize(const LayoutObject*) const { return IntSize(); }

    bool isPending() const { return false; }
    bool knownToBeOpaque(const LayoutObject*) const;

    void loadSubimages(Document*) { }

    DECLARE_TRACE_AFTER_DISPATCH();

protected:
    CSSGradientValue(ClassType classType, CSSGradientRepeat repeat, CSSGradientType gradientType)
        : CSSImageGeneratorValue(classType)
        , m_stopsSorted(false)
        , m_gradientType(gradientType)
        , m_repeating(repeat == Repeating)
    {
    }

    void addStops(Gradient*, const CSSToLengthConversionData&, const LayoutObject&);
    void addDeprecatedStops(Gradient*, const LayoutObject&);

    // Resolve points/radii to front end values.
    FloatPoint computeEndPoint(CSSValue*, CSSValue*, const CSSToLengthConversionData&, const IntSize&);

    bool isCacheable() const;

    // Points. Some of these may be null.
    RefPtrWillBeMember<CSSValue> m_firstX;
    RefPtrWillBeMember<CSSValue> m_firstY;

    RefPtrWillBeMember<CSSValue> m_secondX;
    RefPtrWillBeMember<CSSValue> m_secondY;

    // Stops
    WillBeHeapVector<CSSGradientColorStop, 2> m_stops;
    bool m_stopsSorted;
    CSSGradientType m_gradientType;
    bool m_repeating;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSGradientValue, isGradientValue());

class CSSLinearGradientValue final : public CSSGradientValue {
public:

    static PassRefPtrWillBeRawPtr<CSSLinearGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSLinearGradient)
    {
        return adoptRefWillBeNoop(new CSSLinearGradientValue(repeat, gradientType));
    }

    void setAngle(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_angle = val; }

    String customCSSText() const;

    // Create the gradient for a given size.
    PassRefPtr<Gradient> createGradient(const CSSToLengthConversionData&, const IntSize&, const LayoutObject&);

    bool equals(const CSSLinearGradientValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSLinearGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSLinearGradient)
        : CSSGradientValue(LinearGradientClass, repeat, gradientType)
    {
    }

    RefPtrWillBeMember<CSSPrimitiveValue> m_angle; // may be null.
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSLinearGradientValue, isLinearGradientValue());

class CSSRadialGradientValue final : public CSSGradientValue {
public:
    static PassRefPtrWillBeRawPtr<CSSRadialGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSRadialGradient)
    {
        return adoptRefWillBeNoop(new CSSRadialGradientValue(repeat, gradientType));
    }

    String customCSSText() const;

    void setFirstRadius(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_firstRadius = val; }
    void setSecondRadius(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_secondRadius = val; }

    void setShape(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_shape = val; }
    void setSizingBehavior(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_sizingBehavior = val; }

    void setEndHorizontalSize(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_endHorizontalSize = val; }
    void setEndVerticalSize(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_endVerticalSize = val; }

    // Create the gradient for a given size.
    PassRefPtr<Gradient> createGradient(const CSSToLengthConversionData&, const IntSize&, const LayoutObject&);

    bool equals(const CSSRadialGradientValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSRadialGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSRadialGradient)
        : CSSGradientValue(RadialGradientClass, repeat, gradientType)
    {
    }

    // Resolve points/radii to front end values.
    float resolveRadius(CSSPrimitiveValue*, const CSSToLengthConversionData&, float* widthOrHeight = 0);

    // These may be null for non-deprecated gradients.
    RefPtrWillBeMember<CSSPrimitiveValue> m_firstRadius;
    RefPtrWillBeMember<CSSPrimitiveValue> m_secondRadius;

    // The below are only used for non-deprecated gradients. Any of them may be null.
    RefPtrWillBeMember<CSSPrimitiveValue> m_shape;
    RefPtrWillBeMember<CSSPrimitiveValue> m_sizingBehavior;

    RefPtrWillBeMember<CSSPrimitiveValue> m_endHorizontalSize;
    RefPtrWillBeMember<CSSPrimitiveValue> m_endVerticalSize;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSRadialGradientValue, isRadialGradientValue());

} // namespace blink

#endif // CSSGradientValue_h
