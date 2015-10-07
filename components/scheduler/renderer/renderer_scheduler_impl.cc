// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/child/scheduler_task_runner_delegate.h"

namespace scheduler {
namespace {
const int kLoadingTaskEstimationSampleCount = 200;
const double kLoadingTaskEstimationPercentile = 90;
const int kTimerTaskEstimationSampleCount = 200;
const double kTimerTaskEstimationPercentile = 90;
const int kShortIdlePeriodDurationSampleCount = 10;
const double kShortIdlePeriodDurationPercentile = 20;
}

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner)
    : helper_(main_task_runner,
              "renderer.scheduler",
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug")),
      idle_helper_(&helper_,
                   this,
                   "renderer.scheduler",
                   TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                   "RendererSchedulerIdlePeriod",
                   base::TimeDelta()),
      control_task_runner_(helper_.ControlTaskRunner()),
      compositor_task_runner_(
          helper_.NewTaskQueue(TaskQueue::Spec("compositor_tq")
                                   .SetShouldMonitorQuiescence(true))),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlTaskRunner()),
      policy_may_need_update_(&any_thread_lock_),
      weak_factory_(this) {
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  suspend_timers_when_backgrounded_closure_.Reset(
      base::Bind(&RendererSchedulerImpl::SuspendTimerQueueWhenBackgrounded,
                 weak_factory_.GetWeakPtr()));

  default_loading_task_runner_ = NewLoadingTaskRunner("default_loading_tq");
  default_timer_task_runner_ = NewTimerTaskRunner("default_timer_tq");

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  // Make sure that we don't initially assume there is no idle time.
  MainThreadOnly().short_idle_period_duration.InsertSample(
      cc::BeginFrameArgs::DefaultInterval());

  helper_.SetObserver(this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  for (const scoped_refptr<TaskQueue>& loading_queue : loading_task_runners_) {
    loading_queue->RemoveTaskObserver(
        &MainThreadOnly().loading_task_cost_estimator);
  }
  for (const scoped_refptr<TaskQueue>& timer_queue : timer_task_runners_) {
    timer_queue->RemoveTaskObserver(
        &MainThreadOnly().timer_task_cost_estimator);
  }

  // Ensure the renderer scheduler was shut down explicitly, because otherwise
  // we could end up having stale pointers to the Blink heap which has been
  // terminated by this point.
  DCHECK(MainThreadOnly().was_shutdown);
}

RendererSchedulerImpl::Policy::Policy()
    : compositor_queue_priority(TaskQueue::NORMAL_PRIORITY),
      loading_queue_priority(TaskQueue::NORMAL_PRIORITY),
      timer_queue_priority(TaskQueue::NORMAL_PRIORITY),
      default_queue_priority(TaskQueue::NORMAL_PRIORITY) {}

RendererSchedulerImpl::MainThreadOnly::MainThreadOnly()
    : loading_task_cost_estimator(kLoadingTaskEstimationSampleCount,
                                  kLoadingTaskEstimationPercentile),
      timer_task_cost_estimator(kTimerTaskEstimationSampleCount,
                                kTimerTaskEstimationPercentile),
      short_idle_period_duration(kShortIdlePeriodDurationSampleCount),
      current_use_case(UseCase::NONE),
      timer_queue_suspend_count(0),
      navigation_task_expected_count(0),
      renderer_hidden(false),
      renderer_backgrounded(false),
      timer_queue_suspension_when_backgrounded_enabled(false),
      timer_queue_suspended_when_backgrounded(false),
      was_shutdown(false),
      loading_tasks_seem_expensive(false),
      timer_tasks_seem_expensive(false),
      touchstart_expected_soon(false),
      have_seen_a_begin_main_frame(false) {}

RendererSchedulerImpl::MainThreadOnly::~MainThreadOnly() {}

RendererSchedulerImpl::AnyThread::AnyThread()
    : awaiting_touch_start_response(false),
      in_idle_period(false),
      begin_main_frame_on_critical_path(false) {}

RendererSchedulerImpl::CompositorThreadOnly::CompositorThreadOnly()
    : last_input_type(blink::WebInputEvent::Undefined) {}

RendererSchedulerImpl::CompositorThreadOnly::~CompositorThreadOnly() {
}

void RendererSchedulerImpl::Shutdown() {
  helper_.Shutdown();
  MainThreadOnly().was_shutdown = true;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return default_loading_task_runner_;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::TimerTaskRunner() {
  helper_.CheckOnValidThread();
  return default_timer_task_runner_;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::NewLoadingTaskRunner(
    const char* name) {
  helper_.CheckOnValidThread();
  scoped_refptr<TaskQueue> loading_task_queue(helper_.NewTaskQueue(
      TaskQueue::Spec(name).SetShouldMonitorQuiescence(true)));
  loading_task_runners_.insert(loading_task_queue);
  loading_task_queue->SetQueuePriority(
      MainThreadOnly().current_policy.loading_queue_priority);
  loading_task_queue->AddTaskObserver(
      &MainThreadOnly().loading_task_cost_estimator);
  return loading_task_queue;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::NewTimerTaskRunner(
    const char* name) {
  helper_.CheckOnValidThread();
  scoped_refptr<TaskQueue> timer_task_queue(helper_.NewTaskQueue(
      TaskQueue::Spec(name).SetShouldMonitorQuiescence(true)));
  timer_task_runners_.insert(timer_task_queue);
  timer_task_queue->SetQueuePriority(
      MainThreadOnly().current_policy.timer_queue_priority);
  timer_task_queue->AddTaskObserver(
      &MainThreadOnly().timer_task_cost_estimator);
  return timer_task_queue;
}

void RendererSchedulerImpl::OnUnregisterTaskQueue(
    const scoped_refptr<TaskQueue>& task_queue) {
  if (loading_task_runners_.find(task_queue) != loading_task_runners_.end()) {
    task_queue->RemoveTaskObserver(
        &MainThreadOnly().loading_task_cost_estimator);
    loading_task_runners_.erase(task_queue);
  } else if (timer_task_runners_.find(task_queue) !=
             timer_task_runners_.end()) {
    task_queue->RemoveTaskObserver(&MainThreadOnly().timer_task_cost_estimator);
    timer_task_runners_.erase(task_queue);
  }
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

void RendererSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void RendererSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  EndIdlePeriod();
  MainThreadOnly().estimated_next_frame_begin = args.frame_time + args.interval;
  MainThreadOnly().have_seen_a_begin_main_frame = true;
  {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().begin_main_frame_on_critical_path = args.on_critical_path;
  }
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.Now());
  if (now < MainThreadOnly().estimated_next_frame_begin) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now,
        MainThreadOnly().estimated_next_frame_begin);
    MainThreadOnly().short_idle_period_duration.InsertSample(
        MainThreadOnly().estimated_next_frame_begin - now);
  } else {
    // There was no idle time :(
    MainThreadOnly().short_idle_period_duration.InsertSample(base::TimeDelta());
  }

  MainThreadOnly().expected_short_idle_period_duration =
      MainThreadOnly().short_idle_period_duration.Percentile(
          kShortIdlePeriodDurationPercentile);
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  idle_helper_.EnableLongIdlePeriod();
}

void RendererSchedulerImpl::OnRendererHidden() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererHidden");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || MainThreadOnly().renderer_hidden)
    return;

  idle_helper_.EnableLongIdlePeriod();

  // Ensure that we stop running idle tasks after a few seconds of being hidden.
  end_renderer_hidden_idle_period_closure_.Cancel();
  base::TimeDelta end_idle_when_hidden_delay =
      base::TimeDelta::FromMilliseconds(kEndIdleWhenHiddenDelayMillis);
  control_task_runner_->PostDelayedTask(
      FROM_HERE, end_renderer_hidden_idle_period_closure_.callback(),
      end_idle_when_hidden_delay);
  MainThreadOnly().renderer_hidden = true;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.Now()));
}

void RendererSchedulerImpl::OnRendererVisible() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererVisible");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || !MainThreadOnly().renderer_hidden)
    return;

  end_renderer_hidden_idle_period_closure_.Cancel();
  MainThreadOnly().renderer_hidden = false;
  EndIdlePeriod();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.Now()));
}

void RendererSchedulerImpl::OnRendererBackgrounded() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererBackgrounded");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || MainThreadOnly().renderer_backgrounded)
    return;

  MainThreadOnly().renderer_backgrounded = true;
  if (!MainThreadOnly().timer_queue_suspension_when_backgrounded_enabled)
    return;

  suspend_timers_when_backgrounded_closure_.Cancel();
  base::TimeDelta suspend_timers_when_backgrounded_delay =
      base::TimeDelta::FromMilliseconds(
          kSuspendTimersWhenBackgroundedDelayMillis);
  control_task_runner_->PostDelayedTask(
      FROM_HERE, suspend_timers_when_backgrounded_closure_.callback(),
      suspend_timers_when_backgrounded_delay);
}

void RendererSchedulerImpl::OnRendererForegrounded() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererForegrounded");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || !MainThreadOnly().renderer_backgrounded)
    return;

  MainThreadOnly().renderer_backgrounded = false;
  suspend_timers_when_backgrounded_closure_.Cancel();
  ResumeTimerQueueWhenForegrounded();
}

void RendererSchedulerImpl::EndIdlePeriod() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  idle_helper_.EndIdlePeriod();
}

// static
bool RendererSchedulerImpl::ShouldPrioritizeInputEvent(
    const blink::WebInputEvent& web_input_event) {
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if (web_input_event.type == blink::WebInputEvent::MouseMove &&
      (web_input_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    return true;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::isMouseEventType(web_input_event.type) ||
      blink::WebInputEvent::isKeyboardEventType(web_input_event.type)) {
    return false;
  }
  return true;
}

void RendererSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnCompositorThread");
  if (!ShouldPrioritizeInputEvent(web_input_event))
    return;

  UpdateForInputEventOnCompositorThread(web_input_event.type, event_state);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidAnimateForInputOnCompositorThread");
  UpdateForInputEventOnCompositorThread(
      blink::WebInputEvent::Undefined,
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
}

void RendererSchedulerImpl::UpdateForInputEventOnCompositorThread(
    blink::WebInputEvent::Type type,
    InputEventState input_event_state) {
  base::AutoLock lock(any_thread_lock_);
  base::TimeTicks now = helper_.Now();

  // TODO(alexclarke): Move WebInputEventTraits where we can access it from here
  // and record the name rather than the integer representation.
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::UpdateForInputEventOnCompositorThread",
               "type", static_cast<int>(type), "input_event_state",
               InputEventStateToString(input_event_state));

  bool gesture_already_in_progress = InputSignalsSuggestGestureInProgress(now);
  bool was_awaiting_touch_start_response =
      AnyThread().awaiting_touch_start_response;

  AnyThread().user_model.DidStartProcessingInputEvent(type, now);

  if (input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR)
    AnyThread().user_model.DidFinishProcessingInputEvent(now);

  if (type) {
    switch (type) {
      case blink::WebInputEvent::TouchStart:
        AnyThread().awaiting_touch_start_response = true;
        break;

      case blink::WebInputEvent::TouchMove:
        // Observation of consecutive touchmoves is a strong signal that the
        // page is consuming the touch sequence, in which case touchstart
        // response prioritization is no longer necessary. Otherwise, the
        // initial touchmove should preserve the touchstart response pending
        // state.
        if (AnyThread().awaiting_touch_start_response &&
            CompositorThreadOnly().last_input_type ==
                blink::WebInputEvent::TouchMove) {
          AnyThread().awaiting_touch_start_response = false;
        }
        break;

      case blink::WebInputEvent::Undefined:
      case blink::WebInputEvent::GestureTapDown:
      case blink::WebInputEvent::GestureShowPress:
      case blink::WebInputEvent::GestureFlingCancel:
      case blink::WebInputEvent::GestureScrollEnd:
        // With no observable effect, these meta events do not indicate a
        // meaningful touchstart response and should not impact task priority.
        break;

      default:
        AnyThread().awaiting_touch_start_response = false;
        break;
    }
  }

  // Avoid unnecessary policy updates, while a gesture is already in progress.
  if (!gesture_already_in_progress ||
      was_awaiting_touch_start_response !=
          AnyThread().awaiting_touch_start_response) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  CompositorThreadOnly().last_input_type = type;
}

void RendererSchedulerImpl::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().user_model.DidFinishProcessingInputEvent(helper_.Now());
  }
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart and main-thread gesture use cases indicate a strong
  // likelihood of high-priority work in the near future.
  UseCase use_case = MainThreadOnly().current_use_case;
  return MainThreadOnly().touchstart_expected_soon ||
         use_case == UseCase::TOUCHSTART ||
         use_case == UseCase::MAIN_THREAD_GESTURE;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if there's a urgent task to be run now, or we are expecting
  // one soon (touch start).
  // Note: even though the control queue has the highest priority we don't yield
  // for it since these tasks are not user-provided work and they are only
  // intended to run before the next task, not interrupt the tasks.
  switch (MainThreadOnly().current_use_case) {
    case UseCase::NONE:
      return MainThreadOnly().touchstart_expected_soon;

    case UseCase::COMPOSITOR_GESTURE:
      return MainThreadOnly().touchstart_expected_soon;

    case UseCase::MAIN_THREAD_GESTURE:
      return !compositor_task_runner_->IsQueueEmpty() ||
             MainThreadOnly().touchstart_expected_soon;

    case UseCase::TOUCHSTART:
      return true;

    case UseCase::LOADING:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const tracked_objects::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  any_thread_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_runner_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.Now();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta expected_use_case_duration;
  UseCase use_case = ComputeCurrentUseCase(now, &expected_use_case_duration);
  MainThreadOnly().current_use_case = use_case;

  // TODO(alexclarke): We should wire up a signal from blink to let us know if
  // there are any touch handlers registerd or not, and only call
  // TouchStartExpectedSoon if there is at least one.  NOTE a TouchStart will
  // only actually get sent if there is a touch handler.
  base::TimeDelta touchstart_expected_flag_valid_for_duration;
  bool touchstart_expected_soon = AnyThread().user_model.IsGestureExpectedSoon(
      use_case, now, &touchstart_expected_flag_valid_for_duration);
  MainThreadOnly().touchstart_expected_soon = touchstart_expected_soon;

  bool loading_tasks_seem_expensive =
      MainThreadOnly().loading_task_cost_estimator.expected_task_duration() >
      MainThreadOnly().expected_short_idle_period_duration;
  MainThreadOnly().loading_tasks_seem_expensive = loading_tasks_seem_expensive;

  bool timer_tasks_seem_expensive =
      MainThreadOnly().timer_task_cost_estimator.expected_task_duration() >
      MainThreadOnly().expected_short_idle_period_duration;
  MainThreadOnly().timer_tasks_seem_expensive = timer_tasks_seem_expensive;

  // The |new_policy_duration| is the minimum of |expected_use_case_duration|
  // and |touchstart_expected_flag_valid_for_duration| unless one is zero in
  // which case we choose the other.
  base::TimeDelta new_policy_duration = expected_use_case_duration;
  if (new_policy_duration == base::TimeDelta() ||
      (touchstart_expected_flag_valid_for_duration > base::TimeDelta() &&
       new_policy_duration > touchstart_expected_flag_valid_for_duration)) {
    new_policy_duration = touchstart_expected_flag_valid_for_duration;
  }

  if (new_policy_duration > base::TimeDelta()) {
    MainThreadOnly().current_policy_expiration_time = now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    MainThreadOnly().current_policy_expiration_time = base::TimeTicks();
  }

  Policy new_policy;
  bool block_expensive_tasks = false;
  switch (use_case) {
    case UseCase::COMPOSITOR_GESTURE:
      if (touchstart_expected_soon) {
        block_expensive_tasks = true;
      } else {
        // What we really want to do is priorize loading tasks, but that doesn't
        // seem to be safe. Instead we do that by proxy by deprioritizing
        // compositor tasks. This should be safe since we've already gone to the
        // pain of fixing ordering issues with them.
        new_policy.compositor_queue_priority = TaskQueue::BEST_EFFORT_PRIORITY;
      }
      break;

    case UseCase::MAIN_THREAD_GESTURE:
      new_policy.compositor_queue_priority = TaskQueue::HIGH_PRIORITY;
      block_expensive_tasks = true;
      break;

    case UseCase::TOUCHSTART:
      new_policy.compositor_queue_priority = TaskQueue::HIGH_PRIORITY;
      new_policy.loading_queue_priority = TaskQueue::DISABLED_PRIORITY;
      new_policy.timer_queue_priority = TaskQueue::DISABLED_PRIORITY;
      block_expensive_tasks = true;  // NOTE this is a nop due to the above.
      break;

    case UseCase::NONE:
      if (touchstart_expected_soon)
        block_expensive_tasks = true;
      break;

    case UseCase::LOADING:
      new_policy.loading_queue_priority = TaskQueue::HIGH_PRIORITY;
      new_policy.default_queue_priority = TaskQueue::HIGH_PRIORITY;
      break;

    default:
      NOTREACHED();
  }

  // Don't block expensive tasks unless we have actually seen something.
  if (!MainThreadOnly().have_seen_a_begin_main_frame)
    block_expensive_tasks = false;

  // Don't block expensive tasks if we are expecting a navigation.
  if (MainThreadOnly().navigation_task_expected_count > 0)
    block_expensive_tasks = false;

  if (block_expensive_tasks && loading_tasks_seem_expensive)
    new_policy.loading_queue_priority = TaskQueue::DISABLED_PRIORITY;

  if (MainThreadOnly().timer_queue_suspend_count != 0 ||
      MainThreadOnly().timer_queue_suspended_when_backgrounded) {
    new_policy.timer_queue_priority = TaskQueue::DISABLED_PRIORITY;
  }

  // Tracing is done before the early out check, because it's quite possible we
  // will otherwise miss this information in traces.
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(now));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "use_case",
                 use_case);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.loading_tasks_seem_expensive",
                 MainThreadOnly().loading_tasks_seem_expensive);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.timer_tasks_seem_expensive",
                 MainThreadOnly().timer_tasks_seem_expensive);

  if (update_type == UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED &&
      new_policy == MainThreadOnly().current_policy) {
    return;
  }

  compositor_task_runner_->SetQueuePriority(
      new_policy.compositor_queue_priority);
  for (const scoped_refptr<TaskQueue>& loading_queue : loading_task_runners_) {
    loading_queue->SetQueuePriority(new_policy.loading_queue_priority);
  }
  for (const scoped_refptr<TaskQueue>& timer_queue : timer_task_runners_) {
    timer_queue->SetQueuePriority(new_policy.timer_queue_priority);
  }

  // TODO(alexclarke): We shouldn't have to prioritize the default queue, but it
  // appears to be necessary since the order of loading tasks and IPCs (which
  // are mostly dispatched on the default queue) need to be preserved.
  helper_.DefaultTaskRunner()->SetQueuePriority(
      new_policy.default_queue_priority);

  DCHECK(compositor_task_runner_->IsQueueEnabled());
  MainThreadOnly().current_policy = new_policy;
}

bool RendererSchedulerImpl::InputSignalsSuggestGestureInProgress(
    base::TimeTicks now) const {
  base::TimeDelta unused_policy_duration;
  switch (ComputeCurrentUseCase(now, &unused_policy_duration)) {
    case UseCase::COMPOSITOR_GESTURE:
    case UseCase::MAIN_THREAD_GESTURE:
    case UseCase::TOUCHSTART:
      return true;

    default:
      break;
  }
  return false;
}

RendererSchedulerImpl::UseCase RendererSchedulerImpl::ComputeCurrentUseCase(
    base::TimeTicks now,
    base::TimeDelta* expected_use_case_duration) const {
  any_thread_lock_.AssertAcquired();
  // Above all else we want to be responsive to user input.
  *expected_use_case_duration =
      AnyThread().user_model.TimeLeftInUserGesture(now);
  if (*expected_use_case_duration > base::TimeDelta()) {
    // Has scrolling been fully established?
    if (AnyThread().awaiting_touch_start_response) {
      // No, so arrange for compositor tasks to be run at the highest priority.
      return UseCase::TOUCHSTART;
    }
    // Yes scrolling has been established.  If BeginMainFrame is on the critical
    // path, compositor tasks need to be prioritized, otherwise now might be a
    // good time to run potentially expensive work.
    // TODO(skyostil): Consider removing in_idle_period_ and
    // HadAnIdlePeriodRecently() unless we need them here.
    if (AnyThread().begin_main_frame_on_critical_path) {
      return UseCase::MAIN_THREAD_GESTURE;
    } else {
      return UseCase::COMPOSITOR_GESTURE;
    }
  }

  // TODO(alexclarke): return UseCase::LOADING if signals suggest the system is
  // in the initial 1s of RAIL loading.

  return UseCase::NONE;
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (MainThreadOnly().current_use_case == UseCase::TOUCHSTART) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out =
        MainThreadOnly().current_policy_expiration_time - now;
    return false;
  }
  return true;
}

SchedulerHelper* RendererSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

TaskCostEstimator*
RendererSchedulerImpl::GetLoadingTaskCostEstimatorForTesting() {
  return &MainThreadOnly().loading_task_cost_estimator;
}

TaskCostEstimator*
RendererSchedulerImpl::GetTimerTaskCostEstimatorForTesting() {
  return &MainThreadOnly().timer_task_cost_estimator;
}

void RendererSchedulerImpl::SuspendTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count++;
  ForceUpdatePolicy();
#ifndef NDEBUG
  DCHECK(!default_timer_task_runner_->IsQueueEnabled());
  for (const auto& runner : timer_task_runners_) {
    DCHECK(!runner->IsQueueEnabled());
  }
#endif
}

void RendererSchedulerImpl::ResumeTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count--;
  DCHECK_GE(MainThreadOnly().timer_queue_suspend_count, 0);
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {
  // Note that this will only take effect for the next backgrounded signal.
  MainThreadOnly().timer_queue_suspension_when_backgrounded_enabled = enabled;
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValue(base::TimeTicks optional_now) const {
  base::AutoLock lock(any_thread_lock_);
  return AsValueLocked(optional_now);
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.Now();
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetString("current_use_case",
                   UseCaseToString(MainThreadOnly().current_use_case));
  state->SetBoolean("loading_tasks_seem_expensive",
                    MainThreadOnly().loading_tasks_seem_expensive);
  state->SetBoolean("timer_tasks_seem_expensive",
                    MainThreadOnly().timer_tasks_seem_expensive);
  state->SetBoolean("touchstart_expected_soon",
                    MainThreadOnly().touchstart_expected_soon);
  state->SetString("idle_period_state",
                   IdleHelper::IdlePeriodStateToString(
                       idle_helper_.SchedulerIdlePeriodState()));
  state->SetBoolean("renderer_hidden", MainThreadOnly().renderer_hidden);
  state->SetBoolean("renderer_backgrounded",
                    MainThreadOnly().renderer_backgrounded);
  state->SetBoolean("timer_queue_suspended_when_backgrounded",
                    MainThreadOnly().timer_queue_suspended_when_backgrounded);
  state->SetInteger("timer_queue_suspend_count",
                    MainThreadOnly().timer_queue_suspend_count);
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "rails_loading_priority_deadline",
      (AnyThread().rails_loading_priority_deadline - base::TimeTicks())
          .InMillisecondsF());
  state->SetInteger("navigation_task_expected_count",
                    MainThreadOnly().navigation_task_expected_count);
  state->SetDouble("last_idle_period_end_time",
                   (AnyThread().last_idle_period_end_time - base::TimeTicks())
                       .InMillisecondsF());
  state->SetBoolean("awaiting_touch_start_response",
                    AnyThread().awaiting_touch_start_response);
  state->SetBoolean("begin_main_frame_on_critical_path",
                    AnyThread().begin_main_frame_on_critical_path);
  state->SetDouble("expected_loading_task_duration",
                   MainThreadOnly()
                       .loading_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  state->SetDouble("expected_timer_task_duration",
                   MainThreadOnly()
                       .timer_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  // TODO(skyostil): Can we somehow trace how accurate these estimates were?
  state->SetDouble(
      "expected_short_idle_period_duration",
      MainThreadOnly().expected_short_idle_period_duration.InMillisecondsF());
  state->SetDouble(
      "estimated_next_frame_begin",
      (MainThreadOnly().estimated_next_frame_begin - base::TimeTicks())
          .InMillisecondsF());
  state->SetBoolean("in_idle_period", AnyThread().in_idle_period);
  AnyThread().user_model.AsValueInto(state.get());

  return state;
}

void RendererSchedulerImpl::OnIdlePeriodStarted() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().in_idle_period = true;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnIdlePeriodEnded() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().last_idle_period_end_time = helper_.Now();
  AnyThread().in_idle_period = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::AddPendingNavigation() {
  helper_.CheckOnValidThread();
  MainThreadOnly().navigation_task_expected_count++;
  UpdatePolicy();
}

void RendererSchedulerImpl::RemovePendingNavigation() {
  helper_.CheckOnValidThread();
  DCHECK_GT(MainThreadOnly().navigation_task_expected_count, 0);
  if (MainThreadOnly().navigation_task_expected_count > 0)
    MainThreadOnly().navigation_task_expected_count--;
  UpdatePolicy();
}

void RendererSchedulerImpl::OnNavigationStarted() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnNavigationStarted");
  base::AutoLock lock(any_thread_lock_);
  AnyThread().rails_loading_priority_deadline =
      helper_.Now() + base::TimeDelta::FromMilliseconds(
                          kRailsInitialLoadingPrioritizationMillis);
  ResetForNavigationLocked();
}

bool RendererSchedulerImpl::HadAnIdlePeriodRecently(base::TimeTicks now) const {
  return (now - AnyThread().last_idle_period_end_time) <=
         base::TimeDelta::FromMilliseconds(
             kIdlePeriodStarvationThresholdMillis);
}

void RendererSchedulerImpl::SuspendTimerQueueWhenBackgrounded() {
  DCHECK(MainThreadOnly().renderer_backgrounded);
  if (MainThreadOnly().timer_queue_suspended_when_backgrounded)
    return;

  MainThreadOnly().timer_queue_suspended_when_backgrounded = true;
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::ResumeTimerQueueWhenForegrounded() {
  DCHECK(!MainThreadOnly().renderer_backgrounded);
  if (!MainThreadOnly().timer_queue_suspended_when_backgrounded)
    return;

  MainThreadOnly().timer_queue_suspended_when_backgrounded = false;
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::ResetForNavigationLocked() {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  MainThreadOnly().loading_task_cost_estimator.Clear();
  MainThreadOnly().timer_task_cost_estimator.Clear();
  MainThreadOnly().short_idle_period_duration.Clear();
  // Make sure that we don't initially assume there is no idle time.
  MainThreadOnly().short_idle_period_duration.InsertSample(
      cc::BeginFrameArgs::DefaultInterval());
  AnyThread().user_model.Reset(helper_.Now());
  MainThreadOnly().have_seen_a_begin_main_frame = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

}  // namespace scheduler
