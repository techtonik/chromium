// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_controller.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace ui {
namespace {

gfx::Vector2dF ComputeLineOffsetFromBottom(const SelectionBound& bound) {
  gfx::Vector2dF line_offset =
      gfx::ScaleVector2d(bound.edge_top() - bound.edge_bottom(), 0.5f);
  // An offset of 8 DIPs is sufficient for most line sizes. For small lines,
  // using half the line height avoids synthesizing a point on a line above
  // (or below) the intended line.
  const gfx::Vector2dF kMaxLineOffset(8.f, 8.f);
  line_offset.SetToMin(kMaxLineOffset);
  line_offset.SetToMax(-kMaxLineOffset);
  return line_offset;
}

TouchHandleOrientation ToTouchHandleOrientation(SelectionBound::Type type) {
  switch (type) {
    case SelectionBound::LEFT:
      return TouchHandleOrientation::LEFT;
    case SelectionBound::RIGHT:
      return TouchHandleOrientation::RIGHT;
    case SelectionBound::CENTER:
      return TouchHandleOrientation::CENTER;
    case SelectionBound::EMPTY:
      return TouchHandleOrientation::UNDEFINED;
  }
  NOTREACHED() << "Invalid selection bound type: " << type;
  return TouchHandleOrientation::UNDEFINED;
}

}  // namespace

TouchSelectionController::Config::Config()
    : max_tap_duration(base::TimeDelta::FromMilliseconds(300)),
      tap_slop(8),
      enable_adaptive_handle_orientation(false),
      enable_longpress_drag_selection(false),
      show_on_tap_for_empty_editable(false) {}

TouchSelectionController::Config::~Config() {
}

TouchSelectionController::TouchSelectionController(
    TouchSelectionControllerClient* client,
    const Config& config)
    : client_(client),
      config_(config),
      force_next_update_(false),
      response_pending_input_event_(INPUT_EVENT_TYPE_NONE),
      start_orientation_(TouchHandleOrientation::UNDEFINED),
      end_orientation_(TouchHandleOrientation::UNDEFINED),
      active_status_(INACTIVE),
      activate_insertion_automatically_(false),
      activate_selection_automatically_(false),
      selection_empty_(false),
      selection_editable_(false),
      temporarily_hidden_(false),
      anchor_drag_to_selection_start_(false),
      longpress_drag_selector_(this),
      selection_handle_dragged_(false) {
  DCHECK(client_);
}

TouchSelectionController::~TouchSelectionController() {
}

void TouchSelectionController::OnSelectionBoundsChanged(
    const SelectionBound& start,
    const SelectionBound& end) {
  if (!force_next_update_ && start == start_ && end_ == end)
    return;

  // Notify if selection bounds have just been established or dissolved.
  if (start.type() != SelectionBound::EMPTY &&
      start_.type() == SelectionBound::EMPTY) {
    client_->OnSelectionEvent(SELECTION_ESTABLISHED);
  } else if (start.type() == SelectionBound::EMPTY &&
             start_.type() != SelectionBound::EMPTY) {
    client_->OnSelectionEvent(SELECTION_DISSOLVED);
  }

  start_ = start;
  end_ = end;
  start_orientation_ = ToTouchHandleOrientation(start_.type());
  end_orientation_ = ToTouchHandleOrientation(end_.type());
  force_next_update_ = false;

  if (!activate_selection_automatically_ &&
      !activate_insertion_automatically_) {
    DCHECK_EQ(INACTIVE, active_status_);
    DCHECK_EQ(INPUT_EVENT_TYPE_NONE, response_pending_input_event_);
    return;
  }

  // Ensure that |response_pending_input_event_| is cleared after the method
  // completes, while also making its current value available for the duration
  // of the call.
  InputEventType causal_input_event = response_pending_input_event_;
  response_pending_input_event_ = INPUT_EVENT_TYPE_NONE;
  base::AutoReset<InputEventType> auto_reset_response_pending_input_event(
      &response_pending_input_event_, causal_input_event);

  const bool is_selection_dragging = active_status_ == SELECTION_ACTIVE &&
                                     (start_selection_handle_->IsActive() ||
                                      end_selection_handle_->IsActive());

  // It's possible that the bounds temporarily overlap while a selection handle
  // is being dragged, incorrectly reporting a CENTER orientation.
  // TODO(jdduke): This safeguard is racy, as it's possible the delayed response
  // from handle positioning occurs *after* the handle dragging has ceased.
  // Instead, prevent selection -> insertion transitions without an intervening
  // action or selection clearing of some sort, crbug.com/392696.
  if (is_selection_dragging) {
    if (start_orientation_ == TouchHandleOrientation::CENTER)
      start_orientation_ = start_selection_handle_->orientation();
    if (end_orientation_ == TouchHandleOrientation::CENTER)
      end_orientation_ = end_selection_handle_->orientation();
  }

  if (GetStartPosition() != GetEndPosition() ||
      (is_selection_dragging &&
       start_orientation_ != TouchHandleOrientation::UNDEFINED &&
       end_orientation_ != TouchHandleOrientation::UNDEFINED)) {
    OnSelectionChanged();
    return;
  }

  if (start_orientation_ == TouchHandleOrientation::CENTER &&
      selection_editable_) {
    OnInsertionChanged();
    return;
  }

  HideAndDisallowShowingAutomatically();
}

void TouchSelectionController::OnViewportChanged(
    const gfx::RectF viewport_rect) {
  // Trigger a force update if the viewport is changed, so that
  // it triggers a call to change the mirror values if required.
  if (viewport_rect_ == viewport_rect)
    return;

  viewport_rect_ = viewport_rect;

  if (active_status_ == INACTIVE)
    return;

  if (active_status_ == INSERTION_ACTIVE) {
    DCHECK(insertion_handle_);
    insertion_handle_->SetViewportRect(viewport_rect);
  } else if (active_status_ == SELECTION_ACTIVE) {
    DCHECK(start_selection_handle_);
    DCHECK(end_selection_handle_);
    start_selection_handle_->SetViewportRect(viewport_rect);
    end_selection_handle_->SetViewportRect(viewport_rect);
  }

  // Update handle layout after setting the new Viewport size.
  UpdateHandleLayoutIfNecessary();
}

bool TouchSelectionController::WillHandleTouchEvent(const MotionEvent& event) {
  if (config_.enable_longpress_drag_selection &&
      longpress_drag_selector_.WillHandleTouchEvent(event)) {
    return true;
  }

  if (active_status_ == INSERTION_ACTIVE) {
    DCHECK(insertion_handle_);
    return insertion_handle_->WillHandleTouchEvent(event);
  }

  if (active_status_ == SELECTION_ACTIVE) {
    DCHECK(start_selection_handle_);
    DCHECK(end_selection_handle_);
    if (start_selection_handle_->IsActive())
      return start_selection_handle_->WillHandleTouchEvent(event);

    if (end_selection_handle_->IsActive())
      return end_selection_handle_->WillHandleTouchEvent(event);

    const gfx::PointF event_pos(event.GetX(), event.GetY());
    if ((event_pos - GetStartPosition()).LengthSquared() <=
        (event_pos - GetEndPosition()).LengthSquared()) {
      return start_selection_handle_->WillHandleTouchEvent(event);
    }
    return end_selection_handle_->WillHandleTouchEvent(event);
  }

  return false;
}

bool TouchSelectionController::WillHandleTapEvent(const gfx::PointF& location,
                                                  int tap_count) {
  if (WillHandleTapOrLongPress(location))
    return true;

  if (tap_count > 1) {
    response_pending_input_event_ = REPEATED_TAP;
    ShowSelectionHandlesAutomatically();
  } else {
    response_pending_input_event_ = TAP;
    if (active_status_ != SELECTION_ACTIVE)
      activate_selection_automatically_ = false;
  }
  ShowInsertionHandleAutomatically();
  if (selection_empty_ && !config_.show_on_tap_for_empty_editable)
    DeactivateInsertion();
  ForceNextUpdateIfInactive();
  return false;
}

bool TouchSelectionController::WillHandleLongPressEvent(
    base::TimeTicks event_time,
    const gfx::PointF& location) {
  if (WillHandleTapOrLongPress(location))
    return true;

  longpress_drag_selector_.OnLongPressEvent(event_time, location);
  response_pending_input_event_ = LONG_PRESS;
  ShowSelectionHandlesAutomatically();
  ShowInsertionHandleAutomatically();
  ForceNextUpdateIfInactive();
  return false;
}

void TouchSelectionController::AllowShowingFromCurrentSelection() {
  if (active_status_ != INACTIVE)
    return;

  activate_selection_automatically_ = true;
  activate_insertion_automatically_ = true;
  if (GetStartPosition() != GetEndPosition()) {
    OnSelectionChanged();
  } else if (start_orientation_ == TouchHandleOrientation::CENTER &&
             selection_editable_) {
    OnInsertionChanged();
  }
}

void TouchSelectionController::HideAndDisallowShowingAutomatically() {
  response_pending_input_event_ = INPUT_EVENT_TYPE_NONE;
  DeactivateInsertion();
  DeactivateSelection();
  activate_insertion_automatically_ = false;
  activate_selection_automatically_ = false;
}

void TouchSelectionController::SetTemporarilyHidden(bool hidden) {
  if (temporarily_hidden_ == hidden)
    return;
  temporarily_hidden_ = hidden;
  RefreshHandleVisibility();
}

void TouchSelectionController::OnSelectionEditable(bool editable) {
  if (selection_editable_ == editable)
    return;
  selection_editable_ = editable;
  ForceNextUpdateIfInactive();
  if (!selection_editable_)
    DeactivateInsertion();
}

void TouchSelectionController::OnSelectionEmpty(bool empty) {
  if (selection_empty_ == empty)
    return;
  selection_empty_ = empty;
  ForceNextUpdateIfInactive();
}

bool TouchSelectionController::Animate(base::TimeTicks frame_time) {
  if (active_status_ == INSERTION_ACTIVE)
    return insertion_handle_->Animate(frame_time);

  if (active_status_ == SELECTION_ACTIVE) {
    bool needs_animate = start_selection_handle_->Animate(frame_time);
    needs_animate |= end_selection_handle_->Animate(frame_time);
    return needs_animate;
  }

  return false;
}

gfx::RectF TouchSelectionController::GetRectBetweenBounds() const {
  // Short-circuit for efficiency.
  if (active_status_ == INACTIVE)
    return gfx::RectF();

  if (start_.visible() && !end_.visible())
    return gfx::BoundingRect(start_.edge_top(), start_.edge_bottom());

  if (end_.visible() && !start_.visible())
    return gfx::BoundingRect(end_.edge_top(), end_.edge_bottom());

  // If both handles are visible, or both are invisible, use the entire rect.
  return RectFBetweenSelectionBounds(start_, end_);
}

gfx::RectF TouchSelectionController::GetStartHandleRect() const {
  if (active_status_ == INSERTION_ACTIVE)
    return insertion_handle_->GetVisibleBounds();
  if (active_status_ == SELECTION_ACTIVE)
    return start_selection_handle_->GetVisibleBounds();
  return gfx::RectF();
}

gfx::RectF TouchSelectionController::GetEndHandleRect() const {
  if (active_status_ == INSERTION_ACTIVE)
    return insertion_handle_->GetVisibleBounds();
  if (active_status_ == SELECTION_ACTIVE)
    return end_selection_handle_->GetVisibleBounds();
  return gfx::RectF();
}

const gfx::PointF& TouchSelectionController::GetStartPosition() const {
  return start_.edge_bottom();
}

const gfx::PointF& TouchSelectionController::GetEndPosition() const {
  return end_.edge_bottom();
}

void TouchSelectionController::OnDragBegin(
    const TouchSelectionDraggable& draggable,
    const gfx::PointF& drag_position) {
  if (&draggable == insertion_handle_.get()) {
    DCHECK_EQ(active_status_, INSERTION_ACTIVE);
    client_->OnSelectionEvent(INSERTION_HANDLE_DRAG_STARTED);
    anchor_drag_to_selection_start_ = true;
    return;
  }

  DCHECK_EQ(active_status_, SELECTION_ACTIVE);

  if (&draggable == start_selection_handle_.get()) {
    anchor_drag_to_selection_start_ = true;
  } else if (&draggable == end_selection_handle_.get()) {
    anchor_drag_to_selection_start_ = false;
  } else {
    DCHECK_EQ(&draggable, &longpress_drag_selector_);
    anchor_drag_to_selection_start_ =
        (drag_position - GetStartPosition()).LengthSquared() <
        (drag_position - GetEndPosition()).LengthSquared();
  }

  gfx::PointF base = GetStartPosition() + GetStartLineOffset();
  gfx::PointF extent = GetEndPosition() + GetEndLineOffset();
  if (anchor_drag_to_selection_start_)
    std::swap(base, extent);

  selection_handle_dragged_ = true;

  // When moving the handle we want to move only the extent point. Before doing
  // so we must make sure that the base point is set correctly.
  client_->SelectBetweenCoordinates(base, extent);
  client_->OnSelectionEvent(SELECTION_HANDLE_DRAG_STARTED);
}

void TouchSelectionController::OnDragUpdate(
    const TouchSelectionDraggable& draggable,
    const gfx::PointF& drag_position) {
  // As the position corresponds to the bottom left point of the selection
  // bound, offset it to some reasonable point on the current line of text.
  gfx::Vector2dF line_offset = anchor_drag_to_selection_start_
                                   ? GetStartLineOffset()
                                   : GetEndLineOffset();
  gfx::PointF line_position = drag_position + line_offset;
  if (&draggable == insertion_handle_.get())
    client_->MoveCaret(line_position);
  else
    client_->MoveRangeSelectionExtent(line_position);
}

void TouchSelectionController::OnDragEnd(
    const TouchSelectionDraggable& draggable) {
  if (&draggable == insertion_handle_.get())
    client_->OnSelectionEvent(INSERTION_HANDLE_DRAG_STOPPED);
  else
    client_->OnSelectionEvent(SELECTION_HANDLE_DRAG_STOPPED);
}

bool TouchSelectionController::IsWithinTapSlop(
    const gfx::Vector2dF& delta) const {
  return delta.LengthSquared() <
         (static_cast<double>(config_.tap_slop) * config_.tap_slop);
}

void TouchSelectionController::OnHandleTapped(const TouchHandle& handle) {
  if (insertion_handle_ && &handle == insertion_handle_.get())
    client_->OnSelectionEvent(INSERTION_HANDLE_TAPPED);
}

void TouchSelectionController::SetNeedsAnimate() {
  client_->SetNeedsAnimate();
}

scoped_ptr<TouchHandleDrawable> TouchSelectionController::CreateDrawable() {
  return client_->CreateDrawable();
}

base::TimeDelta TouchSelectionController::GetMaxTapDuration() const {
  return config_.max_tap_duration;
}

bool TouchSelectionController::IsAdaptiveHandleOrientationEnabled() const {
  return config_.enable_adaptive_handle_orientation;
}

void TouchSelectionController::OnLongPressDragActiveStateChanged() {
  // The handles should remain hidden for the duration of a longpress drag,
  // including the time between a longpress and the start of drag motion.
  RefreshHandleVisibility();
}

gfx::PointF TouchSelectionController::GetSelectionStart() const {
  return GetStartPosition();
}

gfx::PointF TouchSelectionController::GetSelectionEnd() const {
  return GetEndPosition();
}

void TouchSelectionController::ShowInsertionHandleAutomatically() {
  if (activate_insertion_automatically_)
    return;
  activate_insertion_automatically_ = true;
  ForceNextUpdateIfInactive();
}

void TouchSelectionController::ShowSelectionHandlesAutomatically() {
  if (activate_selection_automatically_)
    return;
  activate_selection_automatically_ = true;
  ForceNextUpdateIfInactive();
}

bool TouchSelectionController::WillHandleTapOrLongPress(
    const gfx::PointF& location) {
  // If there is an active selection that was not triggered by a user gesture,
  // allow showing the handles for that selection if a gesture occurs within
  // the selection rect. Note that this hit test is at best a crude
  // approximation, and may swallow taps that actually fall outside the
  // real selection.
  if (active_status_ == INACTIVE &&
      GetStartPosition() != GetEndPosition() &&
      RectFBetweenSelectionBounds(start_, end_).Contains(location)) {
    AllowShowingFromCurrentSelection();
    return true;
  }
  return false;
}

void TouchSelectionController::OnInsertionChanged() {
  DeactivateSelection();

  if ((response_pending_input_event_ == TAP ||
       response_pending_input_event_ == REPEATED_TAP) &&
      selection_empty_ && !config_.show_on_tap_for_empty_editable) {
    HideAndDisallowShowingAutomatically();
    return;
  }

  if (!activate_insertion_automatically_)
    return;

  const bool activated = ActivateInsertionIfNecessary();

  const TouchHandle::AnimationStyle animation = GetAnimationStyle(!activated);
  insertion_handle_->SetFocus(start_.edge_top(), start_.edge_bottom());
  insertion_handle_->SetVisible(GetStartVisible(), animation);

  UpdateHandleLayoutIfNecessary();

  client_->OnSelectionEvent(activated ? INSERTION_HANDLE_SHOWN
                                      : INSERTION_HANDLE_MOVED);
}

void TouchSelectionController::OnSelectionChanged() {
  DeactivateInsertion();

  if (!activate_selection_automatically_)
    return;

  const bool activated = ActivateSelectionIfNecessary();

  const TouchHandle::AnimationStyle animation = GetAnimationStyle(!activated);

  start_selection_handle_->SetFocus(start_.edge_top(), start_.edge_bottom());
  end_selection_handle_->SetFocus(end_.edge_top(), end_.edge_bottom());

  start_selection_handle_->SetOrientation(start_orientation_);
  end_selection_handle_->SetOrientation(end_orientation_);

  start_selection_handle_->SetVisible(GetStartVisible(), animation);
  end_selection_handle_->SetVisible(GetEndVisible(), animation);

  UpdateHandleLayoutIfNecessary();

  client_->OnSelectionEvent(activated ? SELECTION_HANDLES_SHOWN
                                      : SELECTION_HANDLES_MOVED);
}

bool TouchSelectionController::ActivateInsertionIfNecessary() {
  DCHECK_NE(SELECTION_ACTIVE, active_status_);

  if (!insertion_handle_) {
    insertion_handle_.reset(
        new TouchHandle(this, TouchHandleOrientation::CENTER, viewport_rect_));
  }

  if (active_status_ == INACTIVE) {
    active_status_ = INSERTION_ACTIVE;
    insertion_handle_->SetEnabled(true);
    insertion_handle_->SetViewportRect(viewport_rect_);
    return true;
  }
  return false;
}

void TouchSelectionController::DeactivateInsertion() {
  if (active_status_ != INSERTION_ACTIVE)
    return;
  DCHECK(insertion_handle_);
  active_status_ = INACTIVE;
  insertion_handle_->SetEnabled(false);
  client_->OnSelectionEvent(INSERTION_HANDLE_CLEARED);
}

bool TouchSelectionController::ActivateSelectionIfNecessary() {
  DCHECK_NE(INSERTION_ACTIVE, active_status_);

  if (!start_selection_handle_) {
    start_selection_handle_.reset(
        new TouchHandle(this, start_orientation_, viewport_rect_));
  } else {
    start_selection_handle_->SetEnabled(true);
    start_selection_handle_->SetViewportRect(viewport_rect_);
  }

  if (!end_selection_handle_) {
    end_selection_handle_.reset(
        new TouchHandle(this, end_orientation_, viewport_rect_));
  } else {
    end_selection_handle_->SetEnabled(true);
    end_selection_handle_->SetViewportRect(viewport_rect_);
  }

  // As a long press received while a selection is already active may trigger
  // an entirely new selection, notify the client but avoid sending an
  // intervening SELECTION_HANDLES_CLEARED update to avoid unnecessary state
  // changes.
  if (active_status_ == INACTIVE ||
      response_pending_input_event_ == LONG_PRESS ||
      response_pending_input_event_ == REPEATED_TAP) {
    if (active_status_ == SELECTION_ACTIVE) {
      // The active selection session finishes with the start of the new one.
      LogSelectionEnd();
    }
    active_status_ = SELECTION_ACTIVE;
    selection_handle_dragged_ = false;
    selection_start_time_ = base::TimeTicks::Now();
    response_pending_input_event_ = INPUT_EVENT_TYPE_NONE;
    longpress_drag_selector_.OnSelectionActivated();
    return true;
  }
  return false;
}

void TouchSelectionController::DeactivateSelection() {
  if (active_status_ != SELECTION_ACTIVE)
    return;
  DCHECK(start_selection_handle_);
  DCHECK(end_selection_handle_);
  LogSelectionEnd();
  longpress_drag_selector_.OnSelectionDeactivated();
  start_selection_handle_->SetEnabled(false);
  end_selection_handle_->SetEnabled(false);
  active_status_ = INACTIVE;
  client_->OnSelectionEvent(SELECTION_HANDLES_CLEARED);
}

void TouchSelectionController::ForceNextUpdateIfInactive() {
  // Only force the update if the reported selection is non-empty but still
  // considered "inactive", i.e., it wasn't preceded by a user gesture or
  // the handles have since been explicitly hidden.
  if (active_status_ == INACTIVE &&
      start_.type() != SelectionBound::EMPTY &&
      end_.type() != SelectionBound::EMPTY) {
    force_next_update_ = true;
  }
}

void TouchSelectionController::UpdateHandleLayoutIfNecessary() {
  if (active_status_ == INSERTION_ACTIVE) {
    DCHECK(insertion_handle_);
    insertion_handle_->UpdateHandleLayout();
  } else if (active_status_ == SELECTION_ACTIVE) {
    DCHECK(start_selection_handle_);
    DCHECK(end_selection_handle_);
    start_selection_handle_->UpdateHandleLayout();
    end_selection_handle_->UpdateHandleLayout();
  }
}

void TouchSelectionController::RefreshHandleVisibility() {
  TouchHandle::AnimationStyle animation_style = GetAnimationStyle(true);
  if (active_status_ == SELECTION_ACTIVE) {
    start_selection_handle_->SetVisible(GetStartVisible(), animation_style);
    end_selection_handle_->SetVisible(GetEndVisible(), animation_style);
  }
  if (active_status_ == INSERTION_ACTIVE)
    insertion_handle_->SetVisible(GetStartVisible(), animation_style);

  // Update handle layout if handle visibility is explicitly changed.
  UpdateHandleLayoutIfNecessary();
}

gfx::Vector2dF TouchSelectionController::GetStartLineOffset() const {
  return ComputeLineOffsetFromBottom(start_);
}

gfx::Vector2dF TouchSelectionController::GetEndLineOffset() const {
  return ComputeLineOffsetFromBottom(end_);
}

bool TouchSelectionController::GetStartVisible() const {
  if (!start_.visible())
    return false;

  return !temporarily_hidden_ && !longpress_drag_selector_.IsActive();
}

bool TouchSelectionController::GetEndVisible() const {
  if (!end_.visible())
    return false;

  return !temporarily_hidden_ && !longpress_drag_selector_.IsActive();
}

TouchHandle::AnimationStyle TouchSelectionController::GetAnimationStyle(
    bool was_active) const {
  return was_active && client_->SupportsAnimation()
             ? TouchHandle::ANIMATION_SMOOTH
             : TouchHandle::ANIMATION_NONE;
}

void TouchSelectionController::LogSelectionEnd() {
  // TODO(mfomitchev): Once we are able to tell the difference between
  // 'successful' and 'unsuccessful' selections - log
  // Event.TouchSelection.Duration instead and get rid of
  // Event.TouchSelectionD.WasDraggeduration.
  if (selection_handle_dragged_) {
    base::TimeDelta duration = base::TimeTicks::Now() - selection_start_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Event.TouchSelection.WasDraggedDuration",
                               duration,
                               base::TimeDelta::FromMilliseconds(500),
                               base::TimeDelta::FromSeconds(60),
                               60);
  }
}

}  // namespace ui
