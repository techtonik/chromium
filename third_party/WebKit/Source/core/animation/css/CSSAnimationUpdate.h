// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAnimationUpdate_h
#define CSSAnimationUpdate_h

#include "core/animation/AnimationStack.h"
#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSPropertyEquality.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/layout/LayoutObject.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Animation;

// This class stores the CSS Animations/Transitions information we use during a style recalc.
// This includes updates to animations/transitions as well as the Interpolations to be applied.
class CSSAnimationUpdate final {
    DISALLOW_ALLOCATION();
    WTF_MAKE_NONCOPYABLE(CSSAnimationUpdate);
public:
    class NewAnimation {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        NewAnimation()
            : styleRuleVersion(0)
        {
        }

        NewAnimation(AtomicString name, InertEffect* effect, Timing timing, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
            : name(name)
            , effect(effect)
            , timing(timing)
            , styleRule(styleRule)
            , styleRuleVersion(this->styleRule->version())
        {
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(effect);
            visitor->trace(styleRule);
        }

        AtomicString name;
        Member<InertEffect> effect;
        Timing timing;
        RefPtrWillBeMember<StyleRuleKeyframes> styleRule;
        unsigned styleRuleVersion;
    };

    class UpdatedAnimation {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        UpdatedAnimation()
            : styleRuleVersion(0)
        {
        }

        UpdatedAnimation(AtomicString name, Animation* animation, InertEffect* effect, Timing specifiedTiming, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
            : name(name)
            , animation(animation)
            , effect(effect)
            , specifiedTiming(specifiedTiming)
            , styleRule(styleRule)
            , styleRuleVersion(this->styleRule->version())
        {
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(animation);
            visitor->trace(effect);
            visitor->trace(styleRule);
        }

        AtomicString name;
        Member<Animation> animation;
        Member<InertEffect> effect;
        Timing specifiedTiming;
        RefPtrWillBeMember<StyleRuleKeyframes> styleRule;
        unsigned styleRuleVersion;
    };

    CSSAnimationUpdate()
    {
    }

    ~CSSAnimationUpdate()
    {
        // For performance reasons, explicitly clear HeapVectors and
        // HeapHashMaps to avoid giving a pressure on Oilpan's GC.
        clear();
    }

    void copy(const CSSAnimationUpdate& update)
    {
        ASSERT(isEmpty());
        m_newAnimations = update.newAnimations();
        m_animationsWithUpdates = update.animationsWithUpdates();
        m_newTransitions = update.newTransitions();
        m_activeInterpolationsForAnimations = update.activeInterpolationsForAnimations();
        m_activeInterpolationsForTransitions = update.activeInterpolationsForTransitions();
        m_cancelledAnimationNames = update.cancelledAnimationNames();
        m_animationsWithPauseToggled = update.animationsWithPauseToggled();
        m_cancelledTransitions = update.cancelledTransitions();
        m_finishedTransitions = update.finishedTransitions();
        m_updatedCompositorKeyframes = update.updatedCompositorKeyframes();
    }

    void clear()
    {
        m_newAnimations.clear();
        m_animationsWithUpdates.clear();
        m_newTransitions.clear();
        m_activeInterpolationsForAnimations.clear();
        m_activeInterpolationsForTransitions.clear();
        m_cancelledAnimationNames.clear();
        m_animationsWithPauseToggled.clear();
        m_cancelledTransitions.clear();
        m_finishedTransitions.clear();
        m_updatedCompositorKeyframes.clear();
    }

    void startAnimation(const AtomicString& animationName, InertEffect* effect, const Timing& timing, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
    {
        effect->setName(animationName);
        m_newAnimations.append(NewAnimation(animationName, effect, timing, styleRule));
    }
    // Returns whether animation has been suppressed and should be filtered during style application.
    bool isSuppressedAnimation(const Animation* animation) const { return m_suppressedAnimations.contains(animation); }
    void cancelAnimation(const AtomicString& name, Animation& animation)
    {
        m_cancelledAnimationNames.append(name);
        m_suppressedAnimations.add(&animation);
    }
    void toggleAnimationPaused(const AtomicString& name)
    {
        m_animationsWithPauseToggled.append(name);
    }
    void updateAnimation(const AtomicString& name, Animation* animation, InertEffect* effect, const Timing& specifiedTiming,
        PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
    {
        m_animationsWithUpdates.append(UpdatedAnimation(name, animation, effect, specifiedTiming, styleRule));
        m_suppressedAnimations.add(animation);
    }
    void updateCompositorKeyframes(Animation* animation)
    {
        m_updatedCompositorKeyframes.append(animation);
    }

    void startTransition(CSSPropertyID id, const AnimatableValue* from, const AnimatableValue* to, InertEffect* effect)
    {
        effect->setName(getPropertyName(id));
        NewTransition newTransition;
        newTransition.id = id;
        newTransition.from = from;
        newTransition.to = to;
        newTransition.effect = effect;
        m_newTransitions.set(id, newTransition);
    }
    bool isCancelledTransition(CSSPropertyID id) const { return m_cancelledTransitions.contains(id); }
    void cancelTransition(CSSPropertyID id) { m_cancelledTransitions.add(id); }
    void finishTransition(CSSPropertyID id) { m_finishedTransitions.add(id); }

    const HeapVector<NewAnimation>& newAnimations() const { return m_newAnimations; }
    const Vector<AtomicString>& cancelledAnimationNames() const { return m_cancelledAnimationNames; }
    const HeapHashSet<Member<const Animation>>& suppressedAnimations() const { return m_suppressedAnimations; }
    const Vector<AtomicString>& animationsWithPauseToggled() const { return m_animationsWithPauseToggled; }
    const HeapVector<UpdatedAnimation>& animationsWithUpdates() const { return m_animationsWithUpdates; }
    const HeapVector<Member<Animation>>& updatedCompositorKeyframes() const { return m_updatedCompositorKeyframes; }

    struct NewTransition {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(effect);
        }

        CSSPropertyID id;
        const AnimatableValue* from;
        const AnimatableValue* to;
        Member<InertEffect> effect;
    };
    using NewTransitionMap = HeapHashMap<CSSPropertyID, NewTransition>;
    const NewTransitionMap& newTransitions() const { return m_newTransitions; }
    const HashSet<CSSPropertyID>& cancelledTransitions() const { return m_cancelledTransitions; }
    const HashSet<CSSPropertyID>& finishedTransitions() const { return m_finishedTransitions; }

    void adoptActiveInterpolationsForAnimations(ActiveInterpolationsMap& newMap) { newMap.swap(m_activeInterpolationsForAnimations); }
    void adoptActiveInterpolationsForTransitions(ActiveInterpolationsMap& newMap) { newMap.swap(m_activeInterpolationsForTransitions); }
    const ActiveInterpolationsMap& activeInterpolationsForAnimations() const { return m_activeInterpolationsForAnimations; }
    const ActiveInterpolationsMap& activeInterpolationsForTransitions() const { return m_activeInterpolationsForTransitions; }
    ActiveInterpolationsMap& activeInterpolationsForAnimations() { return m_activeInterpolationsForAnimations; }

    bool isEmpty() const
    {
        return m_newAnimations.isEmpty()
            && m_cancelledAnimationNames.isEmpty()
            && m_suppressedAnimations.isEmpty()
            && m_animationsWithPauseToggled.isEmpty()
            && m_animationsWithUpdates.isEmpty()
            && m_newTransitions.isEmpty()
            && m_cancelledTransitions.isEmpty()
            && m_finishedTransitions.isEmpty()
            && m_activeInterpolationsForAnimations.isEmpty()
            && m_activeInterpolationsForTransitions.isEmpty()
            && m_updatedCompositorKeyframes.isEmpty();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_newTransitions);
        visitor->trace(m_newAnimations);
        visitor->trace(m_suppressedAnimations);
        visitor->trace(m_animationsWithUpdates);
        visitor->trace(m_updatedCompositorKeyframes);
    }

private:
    // Order is significant since it defines the order in which new animations
    // will be started. Note that there may be multiple animations present
    // with the same name, due to the way in which we split up animations with
    // incomplete keyframes.
    HeapVector<NewAnimation> m_newAnimations;
    Vector<AtomicString> m_cancelledAnimationNames;
    HeapHashSet<Member<const Animation>> m_suppressedAnimations;
    Vector<AtomicString> m_animationsWithPauseToggled;
    HeapVector<UpdatedAnimation> m_animationsWithUpdates;
    HeapVector<Member<Animation>> m_updatedCompositorKeyframes;

    NewTransitionMap m_newTransitions;
    HashSet<CSSPropertyID> m_cancelledTransitions;
    HashSet<CSSPropertyID> m_finishedTransitions;

    ActiveInterpolationsMap m_activeInterpolationsForAnimations;
    ActiveInterpolationsMap m_activeInterpolationsForTransitions;

    friend class PendingAnimationUpdate;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::NewAnimation);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::UpdatedAnimation);

#endif
