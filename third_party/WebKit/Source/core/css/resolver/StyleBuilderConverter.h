/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleBuilderConverter_h
#define StyleBuilderConverter_h

#include "core/css/CSSStringValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/layout/LayoutView.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleReflection.h"
#include "core/style/StyleScrollSnapData.h"
#include "core/style/TransformOrigin.h"
#include "platform/LengthSize.h"
#include "platform/fonts/FontDescription.h"
#include "platform/text/TabSize.h"
#include "wtf/Allocator.h"

namespace blink {

class RotateTransformOperation;
class TranslateTransformOperation;
class ScaleTransformOperation;

// Note that we assume the parser only allows valid CSSValue types.
class StyleBuilderConverter {
    STATIC_ONLY(StyleBuilderConverter);
public:
    static PassRefPtr<StyleReflection> convertBoxReflect(StyleResolverState&, const CSSValue&);
    static AtomicString convertFragmentIdentifier(StyleResolverState&, const CSSValue&);
    static Color convertColor(StyleResolverState&, const CSSValue&, bool forVisitedLink = false);
    template <typename T> static T convertComputedLength(StyleResolverState&, const CSSValue&);
    static LengthBox convertClip(StyleResolverState&, const CSSValue&);
    template <typename T> static T convertFlags(StyleResolverState&, const CSSValue&);
    static FontDescription::FamilyDescription convertFontFamily(StyleResolverState&, const CSSValue&);
    static PassRefPtr<FontFeatureSettings> convertFontFeatureSettings(StyleResolverState&, const CSSValue&);
    static FontDescription::Size convertFontSize(StyleResolverState&, const CSSValue&);
    static float convertFontSizeAdjust(StyleResolverState&, const CSSValue&);
    static FontWeight convertFontWeight(StyleResolverState&, const CSSValue&);
    static FontDescription::VariantLigatures convertFontVariantLigatures(StyleResolverState&, const CSSValue&);
    static EGlyphOrientation convertGlyphOrientation(StyleResolverState&, const CSSValue&);
    static StyleSelfAlignmentData convertSelfOrDefaultAlignmentData(StyleResolverState&, const CSSValue&);
    static StyleContentAlignmentData convertContentAlignmentData(StyleResolverState&, const CSSValue&);
    static GridAutoFlow convertGridAutoFlow(StyleResolverState&, const CSSValue&);
    static GridPosition convertGridPosition(StyleResolverState&, const CSSValue&);
    static GridTrackSize convertGridTrackSize(StyleResolverState&, const CSSValue&);
    template <typename T> static T convertLineWidth(StyleResolverState&, const CSSValue&);
    static Length convertLength(const StyleResolverState&, const CSSValue&);
    static UnzoomedLength convertUnzoomedLength(const StyleResolverState&, const CSSValue&);
    static Length convertLengthOrAuto(const StyleResolverState&, const CSSValue&);
    static Length convertLengthSizing(StyleResolverState&, const CSSValue&);
    static Length convertLengthMaxSizing(StyleResolverState&, const CSSValue&);
    static TabSize convertLengthOrTabSpaces(StyleResolverState&, const CSSValue&);
    static Length convertLineHeight(StyleResolverState&, const CSSValue&);
    static float convertNumberOrPercentage(StyleResolverState&, const CSSValue&);
    static LengthPoint convertPosition(StyleResolverState&, const CSSValue&);
    static float convertPerspective(StyleResolverState&, const CSSValue&);
    static LengthPoint convertPerspectiveOrigin(StyleResolverState&, const CSSValue&);
    static Length convertQuirkyLength(StyleResolverState&, const CSSValue&);
    static PassRefPtr<QuotesData> convertQuotes(StyleResolverState&, const CSSValue&);
    static LengthSize convertRadius(StyleResolverState&, const CSSValue&);
    static EPaintOrder convertPaintOrder(StyleResolverState&, const CSSValue&);
    static PassRefPtr<ShadowList> convertShadow(StyleResolverState&, const CSSValue&);
    static PassRefPtrWillBeRawPtr<ShapeValue> convertShapeValue(StyleResolverState&, const CSSValue&);
    static float convertSpacing(StyleResolverState&, const CSSValue&);
    template <CSSValueID IdForNone> static AtomicString convertString(StyleResolverState&, const CSSValue&);
    static PassRefPtr<SVGDashArray> convertStrokeDasharray(StyleResolverState&, const CSSValue&);
    static StyleColor convertStyleColor(StyleResolverState&, const CSSValue&, bool forVisitedLink = false);
    static float convertTextStrokeWidth(StyleResolverState&, const CSSValue&);
    static TransformOrigin convertTransformOrigin(StyleResolverState&, const CSSValue&);

    static void convertGridTrackList(const CSSValue&, Vector<GridTrackSize>&, NamedGridLinesMap&, OrderedNamedGridLines&, StyleResolverState&);
    static void createImplicitNamedGridLinesFromGridArea(const NamedGridAreaMap&, NamedGridLinesMap&, GridTrackSizingDirection);
    static void convertOrderedNamedGridLinesMapToNamedGridLinesMap(const OrderedNamedGridLines&, NamedGridLinesMap&);

    static ScrollSnapPoints convertSnapPoints(StyleResolverState&, const CSSValue&);
    static Vector<LengthPoint> convertSnapCoordinates(StyleResolverState&, const CSSValue&);
    static LengthPoint convertSnapDestination(StyleResolverState&, const CSSValue&);
    static PassRefPtr<TranslateTransformOperation> convertTranslate(StyleResolverState&, const CSSValue&);
    static PassRefPtr<RotateTransformOperation> convertRotate(StyleResolverState&, const CSSValue&);
    static PassRefPtr<ScaleTransformOperation> convertScale(StyleResolverState&, const CSSValue&);
    static RespectImageOrientationEnum convertImageOrientation(StyleResolverState&, const CSSValue&);
};

template <typename T>
T StyleBuilderConverter::convertComputedLength(StyleResolverState& state, const CSSValue& value)
{
    return toCSSPrimitiveValue(value).computeLength<T>(state.cssToLengthConversionData());
}

template <typename T>
T StyleBuilderConverter::convertFlags(StyleResolverState& state, const CSSValue& value)
{
    T flags = static_cast<T>(0);
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone)
        return flags;
    for (auto& flagValue : toCSSValueList(value))
        flags |= toCSSPrimitiveValue(*flagValue);
    return flags;
}

template <typename T>
T StyleBuilderConverter::convertLineWidth(StyleResolverState& state, const CSSValue& value)
{
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    CSSValueID valueID = primitiveValue.getValueID();
    if (valueID == CSSValueThin)
        return 1;
    if (valueID == CSSValueMedium)
        return 3;
    if (valueID == CSSValueThick)
        return 5;
    if (valueID == CSSValueInvalid) {
        // Any original result that was >= 1 should not be allowed to fall below 1.
        // This keeps border lines from vanishing.
        T result = primitiveValue.computeLength<T>(state.cssToLengthConversionData());
        if (state.style()->effectiveZoom() < 1.0f && result < 1.0) {
            T originalLength = primitiveValue.computeLength<T>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            if (originalLength >= 1.0)
                return 1.0;
        }
        return result;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template <CSSValueID IdForNone>
AtomicString StyleBuilderConverter::convertString(StyleResolverState&, const CSSValue& value)
{
    if (value.isStringValue())
        return AtomicString(toCSSStringValue(value).value());
    ASSERT(toCSSPrimitiveValue(value).getValueID() == IdForNone);
    return nullAtom;
}

} // namespace blink

#endif
