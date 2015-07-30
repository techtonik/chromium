// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue_impl.h"

#include "components/scheduler/child/task_queue_manager.h"

namespace scheduler {
namespace internal {

TaskQueueImpl::TaskQueueImpl(
    TaskQueueManager* task_queue_manager,
    const Spec& spec,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : thread_id_(base::PlatformThread::CurrentId()),
      task_queue_manager_(task_queue_manager),
      pump_policy_(spec.pump_policy),
      delayed_task_sequence_number_(0),
      name_(spec.name),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      wakeup_policy_(spec.wakeup_policy),
      set_index_(0),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers) {}

TaskQueueImpl::~TaskQueueImpl() {}

void TaskQueueImpl::WillDeleteTaskQueueManager() {
  base::AutoLock lock(lock_);
  task_queue_manager_ = nullptr;
  delayed_task_queue_ = base::DelayedTaskQueue();
  incoming_queue_ = base::TaskQueue();
  work_queue_ = base::TaskQueue();
}

bool TaskQueueImpl::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(lock_);
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool TaskQueueImpl::PostDelayedTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NORMAL);
}

bool TaskQueueImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NON_NESTABLE);
}

bool TaskQueueImpl::PostDelayedTaskAt(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeTicks desired_run_time) {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;
  LazyNow lazy_now(task_queue_manager_);
  return PostDelayedTaskLocked(&lazy_now, from_here, task, desired_run_time,
                               TaskType::NORMAL);
}

bool TaskQueueImpl::PostDelayedTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    TaskType task_type) {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;
  LazyNow lazy_now(task_queue_manager_);
  base::TimeTicks desired_run_time;
  if (delay > base::TimeDelta())
    desired_run_time = lazy_now.Now() + delay;
  return PostDelayedTaskLocked(&lazy_now, from_here, task, desired_run_time,
                               task_type);
}

bool TaskQueueImpl::PostDelayedTaskLocked(
    LazyNow* lazy_now,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeTicks desired_run_time,
    TaskType task_type) {
  lock_.AssertAcquired();
  DCHECK(task_queue_manager_);

  base::PendingTask pending_task(from_here, task, base::TimeTicks(),
                                 task_type != TaskType::NON_NESTABLE);
  task_queue_manager_->DidQueueTask(pending_task);

  if (!desired_run_time.is_null()) {
    pending_task.delayed_run_time = std::max(lazy_now->Now(), desired_run_time);
    pending_task.sequence_num = delayed_task_sequence_number_++;
    delayed_task_queue_.push(pending_task);
    TraceQueueSize(true);
    // If we changed the topmost task, then it is time to reschedule.
    if (delayed_task_queue_.top().task.Equals(pending_task.task))
      ScheduleDelayedWorkLocked(lazy_now);
    return true;
  }
  EnqueueTaskLocked(pending_task);
  return true;
}

void TaskQueueImpl::MoveReadyDelayedTasksToIncomingQueue() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return;

  LazyNow lazy_now(task_queue_manager_);
  MoveReadyDelayedTasksToIncomingQueueLocked(&lazy_now);
}

void TaskQueueImpl::MoveReadyDelayedTasksToIncomingQueueLocked(
    LazyNow* lazy_now) {
  lock_.AssertAcquired();
  // Enqueue all delayed tasks that should be running now.
  while (!delayed_task_queue_.empty() &&
         delayed_task_queue_.top().delayed_run_time <= lazy_now->Now()) {
    in_flight_kick_delayed_tasks_.erase(
        delayed_task_queue_.top().delayed_run_time);
    EnqueueTaskLocked(delayed_task_queue_.top());
    delayed_task_queue_.pop();
  }
  TraceQueueSize(true);
  ScheduleDelayedWorkLocked(lazy_now);
}

void TaskQueueImpl::ScheduleDelayedWorkLocked(LazyNow* lazy_now) {
  lock_.AssertAcquired();
  // Any remaining tasks are in the future, so queue a task to kick them.
  if (!delayed_task_queue_.empty()) {
    base::TimeTicks next_run_time = delayed_task_queue_.top().delayed_run_time;
    DCHECK_GE(next_run_time, lazy_now->Now());
    // Make sure we don't have more than one
    // MoveReadyDelayedTasksToIncomingQueue posted for a particular scheduled
    // run time (note it's fine to have multiple ones in flight for distinct
    // run times).
    if (in_flight_kick_delayed_tasks_.find(next_run_time) ==
        in_flight_kick_delayed_tasks_.end()) {
      in_flight_kick_delayed_tasks_.insert(next_run_time);
      base::TimeDelta delay = next_run_time - lazy_now->Now();
      task_queue_manager_->PostDelayedTask(
          FROM_HERE,
          Bind(&TaskQueueImpl::MoveReadyDelayedTasksToIncomingQueue, this),
          delay);
    }
  }
}

bool TaskQueueImpl::IsQueueEnabled() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return false;

  return task_queue_manager_->selector_.IsQueueEnabled(this);
}

TaskQueue::QueueState TaskQueueImpl::GetQueueState() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!work_queue_.empty())
    return QueueState::HAS_WORK;

  {
    base::AutoLock lock(lock_);
    if (incoming_queue_.empty()) {
      return QueueState::EMPTY;
    } else {
      return QueueState::NEEDS_PUMPING;
    }
  }
}

bool TaskQueueImpl::TaskIsOlderThanQueuedTasks(const base::PendingTask* task) {
  lock_.AssertAcquired();
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  // Return false if there are no task in the incoming queue.
  if (incoming_queue_.empty())
    return false;

  base::PendingTask oldest_queued_task = incoming_queue_.front();
  DCHECK(oldest_queued_task.delayed_run_time.is_null());
  DCHECK(task->delayed_run_time.is_null());

  // Note: the comparison is correct due to the fact that the PendingTask
  // operator inverts its comparison operation in order to work well in a heap
  // based priority queue.
  return oldest_queued_task < *task;
}

bool TaskQueueImpl::ShouldAutoPumpQueueLocked(
    bool should_trigger_wakeup,
    const base::PendingTask* previous_task) {
  lock_.AssertAcquired();
  if (pump_policy_ == PumpPolicy::MANUAL)
    return false;
  if (pump_policy_ == PumpPolicy::AFTER_WAKEUP &&
      (!should_trigger_wakeup || TaskIsOlderThanQueuedTasks(previous_task)))
    return false;
  if (incoming_queue_.empty())
    return false;
  return true;
}

bool TaskQueueImpl::NextPendingDelayedTaskRunTime(
    base::TimeTicks* next_pending_delayed_task) {
  base::AutoLock lock(lock_);
  if (delayed_task_queue_.empty())
    return false;
  *next_pending_delayed_task = delayed_task_queue_.top().delayed_run_time;
  return true;
}

void TaskQueueImpl::UpdateWorkQueue(LazyNow* lazy_now,
                                    bool should_trigger_wakeup,
                                    const base::PendingTask* previous_task) {
  DCHECK(work_queue_.empty());
  base::AutoLock lock(lock_);
  if (!ShouldAutoPumpQueueLocked(should_trigger_wakeup, previous_task))
    return;
  MoveReadyDelayedTasksToIncomingQueueLocked(lazy_now);
  work_queue_.Swap(&incoming_queue_);
  // |incoming_queue_| is now empty so TaskQueueManager::UpdateQueues no
  // longer needs to consider this queue for reloading.
  task_queue_manager_->UnregisterAsUpdatableTaskQueue(this);
  if (!work_queue_.empty()) {
    DCHECK(task_queue_manager_);
    task_queue_manager_->selector_.GetTaskQueueSets()->OnPushQueue(this);
    TraceQueueSize(true);
  }
}

base::PendingTask TaskQueueImpl::TakeTaskFromWorkQueue() {
  base::PendingTask pending_task = work_queue_.front();
  work_queue_.pop();
  DCHECK(task_queue_manager_);
  task_queue_manager_->selector_.GetTaskQueueSets()->OnPopQueue(this);
  TraceQueueSize(false);
  return pending_task;
}

void TaskQueueImpl::TraceQueueSize(bool is_locked) const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(disabled_by_default_tracing_category_,
                                     &is_tracing);
  if (!is_tracing)
    return;
  if (!is_locked)
    lock_.Acquire();
  else
    lock_.AssertAcquired();
  TRACE_COUNTER1(
      disabled_by_default_tracing_category_, GetName(),
      incoming_queue_.size() + work_queue_.size() + delayed_task_queue_.size());
  if (!is_locked)
    lock_.Release();
}

void TaskQueueImpl::EnqueueTaskLocked(const base::PendingTask& pending_task) {
  lock_.AssertAcquired();
  if (!task_queue_manager_)
    return;
  if (incoming_queue_.empty())
    task_queue_manager_->RegisterAsUpdatableTaskQueue(this);
  if (pump_policy_ == PumpPolicy::AUTO && incoming_queue_.empty())
    task_queue_manager_->MaybePostDoWorkOnMainRunner();
  incoming_queue_.push(pending_task);
  incoming_queue_.back().sequence_num =
      task_queue_manager_->GetNextSequenceNumber();

  if (!pending_task.delayed_run_time.is_null()) {
    // Clear the delayed run time because we've already applied the delay
    // before getting here.
    incoming_queue_.back().delayed_run_time = base::TimeTicks();
  }
  TraceQueueSize(true);
}

void TaskQueueImpl::SetPumpPolicy(PumpPolicy pump_policy) {
  base::AutoLock lock(lock_);
  if (pump_policy == PumpPolicy::AUTO && pump_policy_ != PumpPolicy::AUTO) {
    PumpQueueLocked();
  }
  pump_policy_ = pump_policy;
}

void TaskQueueImpl::PumpQueueLocked() {
  lock_.AssertAcquired();
  if (!task_queue_manager_)
    return;

  LazyNow lazy_now(task_queue_manager_);
  MoveReadyDelayedTasksToIncomingQueueLocked(&lazy_now);

  bool was_empty = work_queue_.empty();
  while (!incoming_queue_.empty()) {
    work_queue_.push(incoming_queue_.front());
    incoming_queue_.pop();
  }
  // |incoming_queue_| is now empty so TaskQueueManager::UpdateQueues no longer
  // needs to consider this queue for reloading.
  task_queue_manager_->UnregisterAsUpdatableTaskQueue(this);
  if (!work_queue_.empty()) {
    if (was_empty)
      task_queue_manager_->selector_.GetTaskQueueSets()->OnPushQueue(this);
    task_queue_manager_->MaybePostDoWorkOnMainRunner();
  }
}

void TaskQueueImpl::PumpQueue() {
  base::AutoLock lock(lock_);
  PumpQueueLocked();
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

bool TaskQueueImpl::GetWorkQueueFrontTaskAge(int* age) const {
  if (work_queue_.empty())
    return false;
  *age = work_queue_.front().sequence_num;
  return true;
}

void TaskQueueImpl::PushTaskOntoWorkQueueForTest(
    const base::PendingTask& task) {
  work_queue_.push(task);
}

void TaskQueueImpl::PopTaskFromWorkQueueForTest() {
  work_queue_.pop();
}

void TaskQueueImpl::SetQueuePriority(QueuePriority priority) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return;

  task_queue_manager_->selector_.SetQueuePriority(this, priority);
}

// static
const char* TaskQueueImpl::PumpPolicyToString(
    TaskQueue::PumpPolicy pump_policy) {
  switch (pump_policy) {
    case TaskQueue::PumpPolicy::AUTO:
      return "auto";
    case TaskQueue::PumpPolicy::AFTER_WAKEUP:
      return "after_wakeup";
    case TaskQueue::PumpPolicy::MANUAL:
      return "manual";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::WakeupPolicyToString(
    TaskQueue::WakeupPolicy wakeup_policy) {
  switch (wakeup_policy) {
    case TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES:
      return "can_wake_other_queues";
    case TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES:
      return "dont_wake_other_queues";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::PriorityToString(QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    case DISABLED_PRIORITY:
      return "disabled";
    default:
      NOTREACHED();
      return nullptr;
  }
}

void TaskQueueImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  base::AutoLock lock(lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetString("pump_policy", PumpPolicyToString(pump_policy_));
  state->SetString("wakeup_policy", WakeupPolicyToString(wakeup_policy_));
  bool verbose_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      disabled_by_default_verbose_tracing_category_, &verbose_tracing_enabled);
  state->SetInteger("incoming_queue_size", incoming_queue_.size());
  state->SetInteger("work_queue_size", work_queue_.size());
  state->SetInteger("delayed_task_queue_size", delayed_task_queue_.size());
  if (verbose_tracing_enabled) {
    state->BeginArray("incoming_queue");
    QueueAsValueInto(incoming_queue_, state);
    state->EndArray();
    state->BeginArray("work_queue");
    QueueAsValueInto(work_queue_, state);
    state->EndArray();
    state->BeginArray("delayed_task_queue");
    QueueAsValueInto(delayed_task_queue_, state);
    state->EndArray();
  }
  state->SetString("priority",
                   PriorityToString(static_cast<QueuePriority>(set_index_)));
  state->EndDictionary();
}

// static
void TaskQueueImpl::QueueAsValueInto(const base::TaskQueue& queue,
                                     base::trace_event::TracedValue* state) {
  base::TaskQueue queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.front(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueueImpl::QueueAsValueInto(const base::DelayedTaskQueue& queue,
                                     base::trace_event::TracedValue* state) {
  base::DelayedTaskQueue queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.top(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueueImpl::TaskAsValueInto(const base::PendingTask& task,
                                    base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMicroseconds() / 1000.0L);
  state->EndDictionary();
}

}  // namespace internal
}  // namespace scheduler
