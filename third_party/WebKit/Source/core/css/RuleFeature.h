/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RuleFeature_h
#define RuleFeature_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class QualifiedName;
class RuleData;
class SpaceSplitString;
class StyleRule;

struct RuleFeature {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    RuleFeature(StyleRule*, unsigned selectorIndex, bool hasDocumentSecurityOrigin);

    DECLARE_TRACE();

    RawPtrWillBeMember<StyleRule> rule;
    unsigned selectorIndex;
    bool hasDocumentSecurityOrigin;
};

using InvalidationSetVector = Vector<RefPtr<InvalidationSet>, 8>;

class CORE_EXPORT RuleFeatureSet {
    DISALLOW_ALLOCATION();
public:
    RuleFeatureSet();
    ~RuleFeatureSet();

    void add(const RuleFeatureSet&);
    void clear();

    void collectFeaturesFromRuleData(const RuleData&);

    bool usesSiblingRules() const { return !siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_metadata.usesFirstLineRules; }
    bool usesWindowInactiveSelector() const { return m_metadata.usesWindowInactiveSelector; }

    unsigned maxDirectAdjacentSelectors() const { return m_metadata.maxDirectAdjacentSelectors; }
    void setMaxDirectAdjacentSelectors(unsigned value)  { m_metadata.maxDirectAdjacentSelectors = std::max(value, m_metadata.maxDirectAdjacentSelectors); }

    bool hasSelectorForAttribute(const AtomicString& attributeName) const
    {
        ASSERT(!attributeName.isEmpty());
        return m_attributeInvalidationSets.contains(attributeName);
    }

    bool hasSelectorForClass(const AtomicString& classValue) const
    {
        ASSERT(!classValue.isEmpty());
        return m_classInvalidationSets.contains(classValue);
    }

    bool hasSelectorForId(const AtomicString& idValue) const { return m_idInvalidationSets.contains(idValue); }

    void collectInvalidationSetsForClass(InvalidationSetVector&, Element&, const AtomicString& className) const;
    void collectInvalidationSetsForId(InvalidationSetVector&, Element&, const AtomicString& id) const;
    void collectInvalidationSetsForAttribute(InvalidationSetVector&, Element&, const QualifiedName& attributeName) const;
    void collectInvalidationSetsForPseudoClass(InvalidationSetVector&, Element&, CSSSelector::PseudoType) const;

    bool hasIdsInSelectors() const
    {
        return m_idInvalidationSets.size() > 0;
    }

    DECLARE_TRACE();

    WillBeHeapVector<RuleFeature> siblingRules;
    WillBeHeapVector<RuleFeature> uncommonAttributeRules;

protected:
    InvalidationSet* invalidationSetForSelector(const CSSSelector&);

private:
    using InvalidationSetMap = HashMap<AtomicString, RefPtr<InvalidationSet>>;
    using PseudoTypeInvalidationSetMap = HashMap<CSSSelector::PseudoType, RefPtr<InvalidationSet>, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    struct FeatureMetadata {
        DISALLOW_ALLOCATION();
        FeatureMetadata()
            : usesFirstLineRules(false)
            , usesWindowInactiveSelector(false)
            , foundSiblingSelector(false)
            , maxDirectAdjacentSelectors(0)
        { }
        void add(const FeatureMetadata& other);
        void clear();

        bool usesFirstLineRules;
        bool usesWindowInactiveSelector;
        bool foundSiblingSelector;
        unsigned maxDirectAdjacentSelectors;
    };

    void collectFeaturesFromSelector(const CSSSelector&, FeatureMetadata&);

    InvalidationSet& ensureClassInvalidationSet(const AtomicString& className);
    InvalidationSet& ensureAttributeInvalidationSet(const AtomicString& attributeName);
    InvalidationSet& ensureIdInvalidationSet(const AtomicString& attributeName);
    InvalidationSet& ensurePseudoInvalidationSet(CSSSelector::PseudoType);

    void updateInvalidationSets(const RuleData&);
    void updateInvalidationSetsForContentAttribute(const RuleData&);

    struct InvalidationSetFeatures {
        DISALLOW_ALLOCATION();
        InvalidationSetFeatures()
            : customPseudoElement(false)
            , hasBeforeOrAfter(false)
            , treeBoundaryCrossing(false)
            , adjacent(false)
            , insertionPointCrossing(false)
            , forceSubtree(false)
        { }

        bool useSubtreeInvalidation() const { return forceSubtree || adjacent; }

        Vector<AtomicString> classes;
        Vector<AtomicString> attributes;
        AtomicString id;
        AtomicString tagName;
        bool customPseudoElement;
        bool hasBeforeOrAfter;
        bool treeBoundaryCrossing;
        bool adjacent;
        bool insertionPointCrossing;
        bool forceSubtree;
    };

    static bool extractInvalidationSetFeature(const CSSSelector&, InvalidationSetFeatures&);

    enum UseFeaturesType { UseFeatures, ForceSubtree };

    std::pair<const CSSSelector*, UseFeaturesType> extractInvalidationSetFeatures(const CSSSelector&, InvalidationSetFeatures&, bool negated);

    void addFeaturesToInvalidationSet(InvalidationSet&, const InvalidationSetFeatures&);
    void addFeaturesToInvalidationSets(const CSSSelector&, InvalidationSetFeatures&);

    void addClassToInvalidationSet(const AtomicString& className, Element&);

    FeatureMetadata m_metadata;
    InvalidationSetMap m_classInvalidationSets;
    InvalidationSetMap m_attributeInvalidationSets;
    InvalidationSetMap m_idInvalidationSets;
    PseudoTypeInvalidationSetMap m_pseudoInvalidationSets;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::RuleFeature);

#endif // RuleFeature_h
