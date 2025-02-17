// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/user_model.h"

#include "base/metrics/histogram_macros.h"

namespace scheduler {

namespace {
// This enum is used to back a histogram, and should therefore be treated as
// append-only.
enum GesturePredictionResult {
  GESTURE_OCCURED_WAS_PREDICTED = 0,
  GESTURE_OCCURED_BUT_NOT_PREDICTED = 1,
  GESTURE_PREDICTED_BUT_DID_NOT_OCCUR = 2,
  GESTURE_PREDICTION_RESULT_COUNT = 3
};

void RecordGesturePrediction(GesturePredictionResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "RendererScheduler.UserModel.GesturePredictedCorrectly", result,
      GESTURE_PREDICTION_RESULT_COUNT);
}

}  // namespace

UserModel::UserModel()
    : pending_input_event_count_(0), is_gesture_expected_(false) {}
UserModel::~UserModel() {}

void UserModel::DidStartProcessingInputEvent(blink::WebInputEvent::Type type,
                                             const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (type == blink::WebInputEvent::TouchStart ||
      type == blink::WebInputEvent::GestureScrollBegin ||
      type == blink::WebInputEvent::GesturePinchBegin) {
    last_gesture_start_time_ = now;

    RecordGesturePrediction(is_gesture_expected_
                                ? GESTURE_OCCURED_WAS_PREDICTED
                                : GESTURE_OCCURED_BUT_NOT_PREDICTED);

    if (!last_reset_time_.is_null()) {
      base::TimeDelta time_since_reset = now - last_reset_time_;
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "RendererScheduler.UserModel.GestureStartTimeSinceModelReset",
          time_since_reset);
    }

    // If there has been a previous gesture, record a UMA metric for the time
    // interval between then and now.
    if (!last_continuous_gesture_time_.is_null()) {
      base::TimeDelta time_since_last_gesture = now - last_gesture_start_time_;
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "RendererScheduler.UserModel.TimeBetweenGestures",
          time_since_last_gesture);
    }
  }

  // We need to track continuous gestures seperatly for scroll detection
  // because taps should not be confused with scrolls.
  if (type == blink::WebInputEvent::GestureScrollBegin ||
      type == blink::WebInputEvent::GestureScrollEnd ||
      type == blink::WebInputEvent::GestureScrollUpdate ||
      type == blink::WebInputEvent::GestureFlingStart ||
      type == blink::WebInputEvent::GestureFlingCancel ||
      type == blink::WebInputEvent::GesturePinchBegin ||
      type == blink::WebInputEvent::GesturePinchEnd ||
      type == blink::WebInputEvent::GesturePinchUpdate) {
    last_continuous_gesture_time_ = now;
  }

  // If the gesture has ended, record a UMA metric that tracks its duration.
  if (type == blink::WebInputEvent::GestureScrollEnd ||
      type == blink::WebInputEvent::GesturePinchEnd) {
    base::TimeDelta duration = now - last_gesture_start_time_;
    UMA_HISTOGRAM_TIMES("RendererScheduler.UserModel.GestureDuration",
                        duration);
  }

  pending_input_event_count_++;
}

void UserModel::DidFinishProcessingInputEvent(const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (pending_input_event_count_ > 0)
    pending_input_event_count_--;
}

base::TimeDelta UserModel::TimeLeftInUserGesture(base::TimeTicks now) const {
  base::TimeDelta escalated_priority_duration =
      base::TimeDelta::FromMilliseconds(kGestureEstimationLimitMillis);

  // If the input event is still pending, go into input prioritized policy and
  // check again later.
  if (pending_input_event_count_ > 0)
    return escalated_priority_duration;
  if (last_input_signal_time_.is_null() ||
      last_input_signal_time_ + escalated_priority_duration < now) {
    return base::TimeDelta();
  }
  return last_input_signal_time_ + escalated_priority_duration - now;
}

bool UserModel::IsGestureExpectedSoon(
    RendererScheduler::UseCase use_case,
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) {
  bool was_gesture_expected = is_gesture_expected_;
  is_gesture_expected_ =
      IsGestureExpectedSoonImpl(use_case, now, prediction_valid_duration);

  // Track when we start expecting a gesture so we can work out later if a
  // gesture actually happened.
  if (!was_gesture_expected && is_gesture_expected_)
    last_gesture_expected_start_time_ = now;

  if (was_gesture_expected && !is_gesture_expected_ &&
      last_gesture_expected_start_time_ > last_gesture_start_time_) {
    RecordGesturePrediction(GESTURE_PREDICTED_BUT_DID_NOT_OCCUR);
  }
  return is_gesture_expected_;
}

bool UserModel::IsGestureExpectedSoonImpl(
    RendererScheduler::UseCase use_case,
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) const {
  if (use_case == RendererScheduler::UseCase::NONE) {
    // If we've scrolled recently then future scrolling is likely.
    base::TimeDelta expect_subsequent_gesture_for =
        base::TimeDelta::FromMilliseconds(kExpectSubsequentGestureMillis);
    if (last_continuous_gesture_time_.is_null() ||
        last_continuous_gesture_time_ + expect_subsequent_gesture_for <= now) {
      return false;
    }
    *prediction_valid_duration =
        last_continuous_gesture_time_ + expect_subsequent_gesture_for - now;
    return true;
  }

  if (use_case == RendererScheduler::UseCase::COMPOSITOR_GESTURE ||
      use_case == RendererScheduler::UseCase::MAIN_THREAD_GESTURE) {
    // If we've only just started scrolling then, then initiating a subsequent
    // gesture is unlikely.
    base::TimeDelta minimum_typical_scroll_duration =
        base::TimeDelta::FromMilliseconds(kMinimumTypicalScrollDurationMillis);
    if (last_gesture_start_time_.is_null() ||
        last_gesture_start_time_ + minimum_typical_scroll_duration <= now) {
      return true;
    }
    *prediction_valid_duration =
        last_gesture_start_time_ + minimum_typical_scroll_duration - now;
    return false;
  }
  return false;
}

void UserModel::Reset(base::TimeTicks now) {
  last_input_signal_time_ = base::TimeTicks();
  last_gesture_start_time_ = base::TimeTicks();
  last_continuous_gesture_time_ = base::TimeTicks();
  last_gesture_expected_start_time_ = base::TimeTicks();
  last_reset_time_ = now;
  is_gesture_expected_ = false;
}

void UserModel::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary("user_model");
  state->SetInteger("pending_input_event_count", pending_input_event_count_);
  state->SetDouble(
      "last_input_signal_time",
      (last_input_signal_time_ - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "last_touchstart_time",
      (last_gesture_start_time_ - base::TimeTicks()).InMillisecondsF());
  state->SetDouble("last_gesture_expected_start_time",
                   (last_gesture_expected_start_time_ - base::TimeTicks())
                       .InMillisecondsF());
  state->SetBoolean("is_gesture_expected", is_gesture_expected_);
  state->EndDictionary();
}

}  // namespace scheduler
