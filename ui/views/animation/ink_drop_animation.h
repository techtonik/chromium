// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
class LayerDelegate;
}  // namespace ui

namespace views {
class CircleLayerDelegate;
class RectangleLayerDelegate;

namespace test {
class InkDropAnimationTestApi;
}  // namespace test

// An ink drop animation that smoothly animates between a circle and a rounded
// rectangle of different sizes for each of the different InkDropStates. The
// final frame for each InkDropState will be bounded by either a |large_size_|
// rectangle or a |small_size_| rectangle.
//
// TODO(bruthig): Document the ink drop ripple on chromium.org and add a link to
// it.
class VIEWS_EXPORT InkDropAnimation {
 public:
  InkDropAnimation(const gfx::Size& large_size,
                   int large_corner_radius,
                   const gfx::Size& small_size,
                   int small_corner_radius);
  ~InkDropAnimation();

  // The root Layer that can be added in to a Layer tree.
  ui::Layer* root_layer() { return root_layer_.get(); }

  InkDropState ink_drop_state() const { return ink_drop_state_; }

  // Animates from the current |ink_drop_state_| to a new |ink_drop_state|. It
  // is possible to animate from any |ink_drop_state_| to any new
  // |ink_drop_state|. Note that some state transitions will also perform an
  // implicit transition to the another state. e.g. AnimateToState(QUICK_ACTION)
  // will implicitly transition to the HIDDEN state.
  void AnimateToState(InkDropState ink_drop_state);

  // Sets the |center_point| of the ink drop layer relative to its parent Layer.
  void SetCenterPoint(const gfx::Point& center_point);

 private:
  friend class test::InkDropAnimationTestApi;

  // Enumeration of the different shapes that compose the ink drop.
  enum PaintedShape {
    TOP_LEFT_CIRCLE = 0,
    TOP_RIGHT_CIRCLE,
    BOTTOM_RIGHT_CIRCLE,
    BOTTOM_LEFT_CIRCLE,
    HORIZONTAL_RECT,
    VERTICAL_RECT,
    // The total number of shapes, not an actual shape.
    PAINTED_SHAPE_COUNT
  };

  // Type that contains a gfx::Tansform for each of the layers required by the
  // ink drop.
  typedef gfx::Transform InkDropTransforms[PAINTED_SHAPE_COUNT];

  // Animates all of the painted shape layers to the specified |transforms| and
  // |opacity|.
  void AnimateToTransforms(
      const InkDropTransforms transforms,
      float opacity,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy);

  // Resets the Transforms on all the owned Layers to a minimum size.
  void ResetTransformsToMinSize();

  // Sets the |transforms| on all of the shape layers. Note that this does not
  // perform any animation.
  void SetTransforms(const InkDropTransforms transforms);

  // Sets the opacity of the ink drop.
  void SetOpacity(float opacity);

  // Updates all of the Transforms in |transforms_out| for a circle of the given
  // |size|.
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| for a rounded rectangle
  // of the given |size| and |corner_radius|.
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| to the current target
  // Transforms of the Layers.
  void GetCurrentTansforms(InkDropTransforms* transforms_out) const;

  // Adds and configures a new |painted_shape| layer to |painted_layers_|.
  void AddPaintLayer(PaintedShape painted_shape);

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be large.
  gfx::Size large_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be large.
  int large_corner_radius_;

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be small.
  gfx::Size small_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be small.
  int small_corner_radius_;

  // ui::LayerDelegate to paint circles for all the circle layers.
  scoped_ptr<CircleLayerDelegate> circle_layer_delegate_;

  // ui::LayerDelegate to paint rectangles for all the rectangle layers.
  scoped_ptr<RectangleLayerDelegate> rect_layer_delegate_;

  // The root layer that parents the animating layers. The root layer is used to
  // manipulate opacity and location, and its children are used to manipulate
  // the different painted shapes that compose the ink drop.
  scoped_ptr<ui::Layer> root_layer_;

  // ui::Layers for all of the painted shape layers that compose the ink drop.
  scoped_ptr<ui::Layer> painted_layers_[PAINTED_SHAPE_COUNT];

  // The current ink drop state.
  InkDropState ink_drop_state_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimation);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_
