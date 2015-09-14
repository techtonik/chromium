// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <algorithm>
#include <deque>
#include <set>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_jpeg_decode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace content {
namespace {

// Number of milliseconds between successive vsync. Many GL commands block
// on vsync, so thresholds for preemption should be multiples of this.
const int64 kVsyncIntervalMs = 17;

// Amount of time that we will wait for an IPC to be processed before
// preempting. After a preemption, we must wait this long before triggering
// another preemption.
const int64 kPreemptWaitTimeMs = 2 * kVsyncIntervalMs;

// Once we trigger a preemption, the maximum duration that we will wait
// before clearing the preemption.
const int64 kMaxPreemptTimeMs = kVsyncIntervalMs;

// Stop the preemption once the time for the longest pending IPC drops
// below this threshold.
const int64 kStopPreemptThresholdMs = kVsyncIntervalMs;

const uint32_t kOutOfOrderNumber = static_cast<uint32_t>(-1);

}  // anonymous namespace

struct GpuChannelMessage {
  uint32_t order_number;
  base::TimeTicks time_received;
  IPC::Message message;

  // TODO(dyen): Temporary sync point data, remove once new sync point lands.
  bool retire_sync_point;
  uint32 sync_point_number;

  GpuChannelMessage(uint32_t order_num, const IPC::Message& msg)
      : order_number(order_num),
        time_received(base::TimeTicks::Now()),
        message(msg),
        retire_sync_point(false),
        sync_point_number(0) {}
};

class GpuChannelMessageQueue
    : public base::RefCountedThreadSafe<GpuChannelMessageQueue> {
 public:
  static scoped_refptr<GpuChannelMessageQueue> Create(
      base::WeakPtr<GpuChannel> gpu_channel,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    return new GpuChannelMessageQueue(gpu_channel, task_runner);
  }

  uint32_t GetUnprocessedOrderNum() {
    base::AutoLock auto_lock(channel_messages_lock_);
    return unprocessed_order_num_;
  }

  void PushBackMessage(uint32_t order_number, const IPC::Message& message) {
    base::AutoLock auto_lock(channel_messages_lock_);
    if (enabled_) {
      PushMessageHelper(order_number,
                        new GpuChannelMessage(order_number, message));
    }
  }

  void PushOutOfOrderMessage(const IPC::Message& message) {
    // These are pushed out of order so should not have any order messages.
    base::AutoLock auto_lock(channel_messages_lock_);
    if (enabled_) {
      PushOutOfOrderHelper(new GpuChannelMessage(kOutOfOrderNumber, message));
    }
  }

  bool GenerateSyncPointMessage(gpu::SyncPointManager* sync_point_manager,
                                uint32_t order_number,
                                const IPC::Message& message,
                                bool retire_sync_point,
                                uint32_t* sync_point_number) {
    DCHECK(message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID);
    base::AutoLock auto_lock(channel_messages_lock_);
    if (enabled_) {
      const uint32 sync_point = sync_point_manager->GenerateSyncPoint();

      GpuChannelMessage* msg = new GpuChannelMessage(order_number, message);
      msg->retire_sync_point = retire_sync_point;
      msg->sync_point_number = sync_point;

      *sync_point_number = sync_point;
      PushMessageHelper(order_number, msg);
      return true;
    }
    return false;
  }

  bool HasQueuedMessages() {
    base::AutoLock auto_lock(channel_messages_lock_);
    return HasQueuedMessagesLocked();
  }

  base::TimeTicks GetNextMessageTimeTick() {
    base::AutoLock auto_lock(channel_messages_lock_);

    base::TimeTicks next_message_tick;
    if (!channel_messages_.empty())
      next_message_tick = channel_messages_.front()->time_received;

    base::TimeTicks next_out_of_order_tick;
    if (!out_of_order_messages_.empty())
      next_out_of_order_tick = out_of_order_messages_.front()->time_received;

    if (next_message_tick.is_null())
      return next_out_of_order_tick;
    else if (next_out_of_order_tick.is_null())
      return next_message_tick;
    else
      return std::min(next_message_tick, next_out_of_order_tick);
  }

 protected:
  virtual ~GpuChannelMessageQueue() {
    DCHECK(channel_messages_.empty());
    DCHECK(out_of_order_messages_.empty());
  }

 private:
  friend class GpuChannel;
  friend class base::RefCountedThreadSafe<GpuChannelMessageQueue>;

  GpuChannelMessageQueue(
      base::WeakPtr<GpuChannel> gpu_channel,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : enabled_(true),
        unprocessed_order_num_(0),
        gpu_channel_(gpu_channel),
        task_runner_(task_runner) {}

  void DeleteAndDisableMessages(GpuChannelManager* gpu_channel_manager) {
    {
      base::AutoLock auto_lock(channel_messages_lock_);
      DCHECK(enabled_);
      enabled_ = false;
    }

    // We guarantee that the queues will no longer be modified after enabled_
    // is set to false, it is now safe to modify the queue without the lock.
    // All public facing modifying functions check enabled_ while all
    // private modifying functions DCHECK(enabled_) to enforce this.
    while (!channel_messages_.empty()) {
      GpuChannelMessage* msg = channel_messages_.front();
      // This needs to clean up both GpuCommandBufferMsg_InsertSyncPoint and
      // GpuCommandBufferMsg_RetireSyncPoint messages, safer to just check
      // if we have a sync point number here.
      if (msg->sync_point_number) {
        gpu_channel_manager->sync_point_manager()->RetireSyncPoint(
            msg->sync_point_number);
      }
      delete msg;
      channel_messages_.pop_front();
    }
    STLDeleteElements(&out_of_order_messages_);
  }

  void PushUnfinishedMessage(uint32_t order_number,
                             const IPC::Message& message) {
    // This is pushed only if it was unfinished, so order number is kept.
    GpuChannelMessage* msg = new GpuChannelMessage(order_number, message);
    base::AutoLock auto_lock(channel_messages_lock_);
    DCHECK(enabled_);
    const bool had_messages = HasQueuedMessagesLocked();
    if (order_number == kOutOfOrderNumber)
      out_of_order_messages_.push_front(msg);
    else
      channel_messages_.push_front(msg);

    if (!had_messages)
      ScheduleHandleMessage();
  }

  void ScheduleHandleMessage() {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuChannel::HandleMessage, gpu_channel_));
  }

  void PushMessageHelper(uint32_t order_number, GpuChannelMessage* msg) {
    channel_messages_lock_.AssertAcquired();
    DCHECK(enabled_);
    unprocessed_order_num_ = order_number;
    const bool had_messages = HasQueuedMessagesLocked();
    channel_messages_.push_back(msg);
    if (!had_messages)
      ScheduleHandleMessage();
  }

  void PushOutOfOrderHelper(GpuChannelMessage* msg) {
    channel_messages_lock_.AssertAcquired();
    DCHECK(enabled_);
    const bool had_messages = HasQueuedMessagesLocked();
    out_of_order_messages_.push_back(msg);
    if (!had_messages)
      ScheduleHandleMessage();
  }

  bool HasQueuedMessagesLocked() {
    channel_messages_lock_.AssertAcquired();
    return !channel_messages_.empty() || !out_of_order_messages_.empty();
  }

  bool enabled_;

  // Highest IPC order number seen, set when queued on the IO thread.
  uint32_t unprocessed_order_num_;
  std::deque<GpuChannelMessage*> channel_messages_;
  std::deque<GpuChannelMessage*> out_of_order_messages_;

  // This lock protects enabled_, unprocessed_order_num_, and both deques.
  base::Lock channel_messages_lock_;

  base::WeakPtr<GpuChannel> gpu_channel_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessageQueue);
};

// Begin order numbers at 1 so 0 can mean no orders.
uint32_t GpuChannelMessageFilter::global_order_counter_ = 1;

GpuChannelMessageFilter::GpuChannelMessageFilter(
    scoped_refptr<GpuChannelMessageQueue> message_queue,
    gpu::SyncPointManager* sync_point_manager,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool future_sync_points)
    : preemption_state_(IDLE),
      message_queue_(message_queue),
      sender_(nullptr),
      peer_pid_(base::kNullProcessId),
      sync_point_manager_(sync_point_manager),
      task_runner_(task_runner),
      a_stub_is_descheduled_(false),
      future_sync_points_(future_sync_points) {}

GpuChannelMessageFilter::~GpuChannelMessageFilter() {}

void GpuChannelMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(!sender_);
  sender_ = sender;
  timer_ = make_scoped_ptr(new base::OneShotTimer<GpuChannelMessageFilter>);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterAdded(sender_);
  }
}

void GpuChannelMessageFilter::OnFilterRemoved() {
  DCHECK(sender_);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterRemoved();
  }
  sender_ = nullptr;
  peer_pid_ = base::kNullProcessId;
  timer_ = nullptr;
}

void GpuChannelMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(peer_pid_ == base::kNullProcessId);
  peer_pid_ = peer_pid;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelConnected(peer_pid);
  }
}

void GpuChannelMessageFilter::OnChannelError() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelError();
  }
}

void GpuChannelMessageFilter::OnChannelClosing() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelClosing();
  }
}

void GpuChannelMessageFilter::AddChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  channel_filters_.push_back(filter);
  if (sender_)
    filter->OnFilterAdded(sender_);
  if (peer_pid_ != base::kNullProcessId)
    filter->OnChannelConnected(peer_pid_);
}

void GpuChannelMessageFilter::RemoveChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  if (sender_)
    filter->OnFilterRemoved();
  channel_filters_.erase(
      std::find(channel_filters_.begin(), channel_filters_.end(), filter));
}

bool GpuChannelMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(sender_);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    if (filter->OnMessageReceived(message)) {
      return true;
    }
  }

  const uint32_t order_number = global_order_counter_++;
  bool handled = false;
  if ((message.type() == GpuCommandBufferMsg_RetireSyncPoint::ID) &&
      !future_sync_points_) {
    DLOG(ERROR) << "Untrusted client should not send "
                   "GpuCommandBufferMsg_RetireSyncPoint message";
    return true;
  }

  if (message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID) {
    base::Tuple<bool> retire;
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    if (!GpuCommandBufferMsg_InsertSyncPoint::ReadSendParam(&message,
                                                            &retire)) {
      reply->set_reply_error();
      Send(reply);
      return true;
    }
    if (!future_sync_points_ && !base::get<0>(retire)) {
      LOG(ERROR) << "Untrusted contexts can't create future sync points";
      reply->set_reply_error();
      Send(reply);
      return true;
    }

    // Message queue must handle the entire sync point generation because the
    // message queue could be disabled from the main thread during generation.
    uint32_t sync_point = 0u;
    if (!message_queue_->GenerateSyncPointMessage(
            sync_point_manager_, order_number, message, base::get<0>(retire),
            &sync_point)) {
      LOG(ERROR) << "GpuChannel has been destroyed.";
      reply->set_reply_error();
      Send(reply);
      return true;
    }

    DCHECK_NE(sync_point, 0u);
    GpuCommandBufferMsg_InsertSyncPoint::WriteReplyParams(reply, sync_point);
    Send(reply);
    handled = true;
  }

  // Forward all other messages to the GPU Channel.
  if (!handled && !message.is_reply() && !message.should_unblock()) {
    if (message.type() == GpuCommandBufferMsg_WaitForTokenInRange::ID ||
        message.type() == GpuCommandBufferMsg_WaitForGetOffsetInRange::ID) {
      // Move Wait commands to the head of the queue, so the renderer
      // doesn't have to wait any longer than necessary.
      message_queue_->PushOutOfOrderMessage(message);
    } else {
      message_queue_->PushBackMessage(order_number, message);
    }
    handled = true;
  }

  UpdatePreemptionState();
  return handled;
}

void GpuChannelMessageFilter::OnMessageProcessed() {
  UpdatePreemptionState();
}

void GpuChannelMessageFilter::SetPreemptingFlagAndSchedulingState(
    gpu::PreemptionFlag* preempting_flag,
    bool a_stub_is_descheduled) {
  preempting_flag_ = preempting_flag;
  a_stub_is_descheduled_ = a_stub_is_descheduled;
}

void GpuChannelMessageFilter::UpdateStubSchedulingState(
    bool a_stub_is_descheduled) {
  a_stub_is_descheduled_ = a_stub_is_descheduled;
  UpdatePreemptionState();
}

bool GpuChannelMessageFilter::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void GpuChannelMessageFilter::UpdatePreemptionState() {
  switch (preemption_state_) {
    case IDLE:
      if (preempting_flag_.get() && message_queue_->HasQueuedMessages())
        TransitionToWaiting();
      break;
    case WAITING:
      // A timer will transition us to CHECKING.
      DCHECK(timer_->IsRunning());
      break;
    case CHECKING: {
      base::TimeTicks time_tick = message_queue_->GetNextMessageTimeTick();
      if (!time_tick.is_null()) {
        base::TimeDelta time_elapsed = base::TimeTicks::Now() - time_tick;
        if (time_elapsed.InMilliseconds() < kPreemptWaitTimeMs) {
          // Schedule another check for when the IPC may go long.
          timer_->Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs) -
                            time_elapsed,
                        this, &GpuChannelMessageFilter::UpdatePreemptionState);
        } else {
          if (a_stub_is_descheduled_)
            TransitionToWouldPreemptDescheduled();
          else
            TransitionToPreempting();
        }
      }
    } break;
    case PREEMPTING:
      // A TransitionToIdle() timer should always be running in this state.
      DCHECK(timer_->IsRunning());
      if (a_stub_is_descheduled_)
        TransitionToWouldPreemptDescheduled();
      else
        TransitionToIdleIfCaughtUp();
      break;
    case WOULD_PREEMPT_DESCHEDULED:
      // A TransitionToIdle() timer should never be running in this state.
      DCHECK(!timer_->IsRunning());
      if (!a_stub_is_descheduled_)
        TransitionToPreempting();
      else
        TransitionToIdleIfCaughtUp();
      break;
    default:
      NOTREACHED();
  }
}

void GpuChannelMessageFilter::TransitionToIdleIfCaughtUp() {
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  base::TimeTicks next_tick = message_queue_->GetNextMessageTimeTick();
  if (next_tick.is_null()) {
    TransitionToIdle();
  } else {
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - next_tick;
    if (time_elapsed.InMilliseconds() < kStopPreemptThresholdMs)
      TransitionToIdle();
  }
}

void GpuChannelMessageFilter::TransitionToIdle() {
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  // Stop any outstanding timer set to force us from PREEMPTING to IDLE.
  timer_->Stop();

  preemption_state_ = IDLE;
  preempting_flag_->Reset();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToWaiting() {
  DCHECK_EQ(preemption_state_, IDLE);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = WAITING;
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs), this,
                &GpuChannelMessageFilter::TransitionToChecking);
}

void GpuChannelMessageFilter::TransitionToChecking() {
  DCHECK_EQ(preemption_state_, WAITING);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = CHECKING;
  max_preemption_time_ = base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs);
  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToPreempting() {
  DCHECK(preemption_state_ == CHECKING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  DCHECK(!a_stub_is_descheduled_);

  // Stop any pending state update checks that we may have queued
  // while CHECKING.
  if (preemption_state_ == CHECKING)
    timer_->Stop();

  preemption_state_ = PREEMPTING;
  preempting_flag_->Set();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 1);

  timer_->Start(FROM_HERE, max_preemption_time_, this,
                &GpuChannelMessageFilter::TransitionToIdle);

  UpdatePreemptionState();
}

void GpuChannelMessageFilter::TransitionToWouldPreemptDescheduled() {
  DCHECK(preemption_state_ == CHECKING || preemption_state_ == PREEMPTING);
  DCHECK(a_stub_is_descheduled_);

  if (preemption_state_ == CHECKING) {
    // Stop any pending state update checks that we may have queued
    // while CHECKING.
    timer_->Stop();
  } else {
    // Stop any TransitionToIdle() timers that we may have queued
    // while PREEMPTING.
    timer_->Stop();
    max_preemption_time_ = timer_->desired_run_time() - base::TimeTicks::Now();
    if (max_preemption_time_ < base::TimeDelta()) {
      TransitionToIdle();
      return;
    }
  }

  preemption_state_ = WOULD_PREEMPT_DESCHEDULED;
  preempting_flag_->Reset();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

  UpdatePreemptionState();
}

GpuChannel::StreamState::StreamState(int32 id, GpuStreamPriority priority)
    : id_(id), priority_(priority) {}

GpuChannel::StreamState::~StreamState() {}

void GpuChannel::StreamState::AddRoute(int32 route_id) {
  routes_.insert(route_id);
}
void GpuChannel::StreamState::RemoveRoute(int32 route_id) {
  routes_.erase(route_id);
}

bool GpuChannel::StreamState::HasRoute(int32 route_id) const {
  return routes_.find(route_id) != routes_.end();
}

bool GpuChannel::StreamState::HasRoutes() const {
  return !routes_.empty();
}

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       GpuWatchdog* watchdog,
                       gfx::GLShareGroup* share_group,
                       gpu::gles2::MailboxManager* mailbox,
                       base::SingleThreadTaskRunner* task_runner,
                       base::SingleThreadTaskRunner* io_task_runner,
                       int client_id,
                       uint64_t client_tracing_id,
                       bool software,
                       bool allow_future_sync_points,
                       bool allow_real_time_streams)
    : gpu_channel_manager_(gpu_channel_manager),
      channel_id_(IPC::Channel::GenerateVerifiedChannelID("gpu")),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      share_group_(share_group ? share_group : new gfx::GLShareGroup),
      mailbox_manager_(mailbox
                           ? scoped_refptr<gpu::gles2::MailboxManager>(mailbox)
                           : gpu::gles2::MailboxManager::Create()),
      subscription_ref_set_(new gpu::gles2::SubscriptionRefSet),
      pending_valuebuffer_state_(new gpu::ValueStateMap),
      watchdog_(watchdog),
      software_(software),
      current_order_num_(0),
      processed_order_num_(0),
      num_stubs_descheduled_(0),
      allow_future_sync_points_(allow_future_sync_points),
      allow_real_time_streams_(allow_real_time_streams),
      weak_factory_(this) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  message_queue_ =
      GpuChannelMessageQueue::Create(weak_factory_.GetWeakPtr(), task_runner);

  filter_ = new GpuChannelMessageFilter(
      message_queue_, gpu_channel_manager_->sync_point_manager(), task_runner_,
      allow_future_sync_points_);

  subscription_ref_set_->AddObserver(this);
}

GpuChannel::~GpuChannel() {
  // Clear stubs first because of dependencies.
  stubs_.clear();

  message_queue_->DeleteAndDisableMessages(gpu_channel_manager_);

  subscription_ref_set_->RemoveObserver(this);
  if (preempting_flag_.get())
    preempting_flag_->Reset();
}

IPC::ChannelHandle GpuChannel::Init(base::WaitableEvent* shutdown_event) {
  DCHECK(shutdown_event);
  DCHECK(!channel_);

  IPC::ChannelHandle channel_handle(channel_id_);

  channel_ =
      IPC::SyncChannel::Create(channel_handle, IPC::Channel::MODE_SERVER, this,
                               io_task_runner_, false, shutdown_event);

#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
  // that it gets closed after it has been sent.
  base::ScopedFD renderer_fd = channel_->TakeClientFileDescriptor();
  DCHECK(renderer_fd.is_valid());
  channel_handle.socket = base::FileDescriptor(renderer_fd.Pass());
#endif

  channel_->AddFilter(filter_.get());

  return channel_handle;
}

base::ProcessId GpuChannel::GetClientPID() const {
  return channel_->GetPeerPID();
}

bool GpuChannel::OnMessageReceived(const IPC::Message& message) {
  // All messages should be pushed to channel_messages_ and handled separately.
  NOTREACHED();
  return false;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());

  DVLOG(1) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();

  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnAddSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_AddSubscription(client_id_, target));
}

void GpuChannel::OnRemoveSubscription(unsigned int target) {
  gpu_channel_manager()->Send(
      new GpuHostMsg_RemoveSubscription(client_id_, target));
}

void GpuChannel::StubSchedulingChanged(bool scheduled) {
  bool a_stub_was_descheduled = num_stubs_descheduled_ > 0;
  if (scheduled) {
    num_stubs_descheduled_--;
    message_queue_->ScheduleHandleMessage();
  } else {
    num_stubs_descheduled_++;
  }
  DCHECK_LE(num_stubs_descheduled_, stubs_.size());
  bool a_stub_is_descheduled = num_stubs_descheduled_ > 0;

  if (a_stub_is_descheduled != a_stub_was_descheduled) {
    if (preempting_flag_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&GpuChannelMessageFilter::UpdateStubSchedulingState,
                     filter_, a_stub_is_descheduled));
    }
  }
}

CreateCommandBufferResult GpuChannel::CreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id) {
  TRACE_EVENT1("gpu",
               "GpuChannel::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  int32 share_group_id = init_params.share_group_id;
  GpuCommandBufferStub* share_group = stubs_.get(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE)
    return CREATE_COMMAND_BUFFER_FAILED;

  int32 stream_id = init_params.stream_id;
  GpuStreamPriority stream_priority = init_params.stream_priority;

  if (share_group && stream_id != share_group->stream_id())
    return CREATE_COMMAND_BUFFER_FAILED;

  if (!allow_real_time_streams_ &&
      stream_priority == GpuStreamPriority::REAL_TIME)
    return CREATE_COMMAND_BUFFER_FAILED;

  auto stream_it = streams_.find(stream_id);
  if (stream_it != streams_.end() &&
      stream_priority != GpuStreamPriority::INHERIT &&
      stream_priority != stream_it->second.priority()) {
    return CREATE_COMMAND_BUFFER_FAILED;
  }

  // Virtualize compositor contexts on OS X to prevent performance regressions
  // when enabling FCM.
  // http://crbug.com/180463
  bool use_virtualized_gl_context = false;
#if defined(OS_MACOSX)
  use_virtualized_gl_context = true;
#endif

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, task_runner_.get(), share_group, window, mailbox_manager_.get(),
      subscription_ref_set_.get(), pending_valuebuffer_state_.get(),
      gfx::Size(), disallowed_features_, init_params.attribs,
      init_params.gpu_preference, use_virtualized_gl_context, stream_id,
      route_id, surface_id, watchdog_, software_, init_params.active_url));

  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);

  if (!router_.AddRoute(route_id, stub.get())) {
    DLOG(ERROR) << "GpuChannel::CreateViewCommandBuffer(): "
                   "failed to add route";
    return CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST;
  }

  if (stream_it != streams_.end()) {
    stream_it->second.AddRoute(route_id);
  } else {
    StreamState stream(stream_id, stream_priority);
    stream.AddRoute(route_id);
    streams_.insert(std::make_pair(stream_id, stream));
  }

  stubs_.set(route_id, stub.Pass());
  return CREATE_COMMAND_BUFFER_SUCCEEDED;
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32 route_id) {
  return stubs_.get(route_id);
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::MarkAllContextsLost() {
  for (auto& kv : stubs_)
    kv.second->MarkContextLost();
}

bool GpuChannel::AddRoute(int32 route_id, IPC::Listener* listener) {
  return router_.AddRoute(route_id, listener);
}

void GpuChannel::RemoveRoute(int32 route_id) {
  router_.RemoveRoute(route_id);
}

gpu::PreemptionFlag* GpuChannel::GetPreemptionFlag() {
  if (!preempting_flag_.get()) {
    preempting_flag_ = new gpu::PreemptionFlag;
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &GpuChannelMessageFilter::SetPreemptingFlagAndSchedulingState,
            filter_, preempting_flag_, num_stubs_descheduled_ > 0));
  }
  return preempting_flag_.get();
}

void GpuChannel::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> preempted_flag) {
  preempted_flag_ = preempted_flag;

  for (auto& kv : stubs_)
    kv.second->SetPreemptByFlag(preempted_flag_);
}

void GpuChannel::OnDestroy() {
  TRACE_EVENT0("gpu", "GpuChannel::OnDestroy");
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
                        OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuMsg_CreateJpegDecoder,
                                    OnCreateJpegDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void GpuChannel::HandleMessage() {
  GpuChannelMessage* m = nullptr;
  GpuCommandBufferStub* stub = nullptr;
  bool has_more_messages = false;
  {
    base::AutoLock auto_lock(message_queue_->channel_messages_lock_);
    if (!message_queue_->out_of_order_messages_.empty()) {
      m = message_queue_->out_of_order_messages_.front();
      DCHECK(m->order_number == kOutOfOrderNumber);
      message_queue_->out_of_order_messages_.pop_front();
    } else if (!message_queue_->channel_messages_.empty()) {
      m = message_queue_->channel_messages_.front();
      DCHECK(m->order_number != kOutOfOrderNumber);
      message_queue_->channel_messages_.pop_front();
    } else {
      // No messages to process
      return;
    }

    has_more_messages = message_queue_->HasQueuedMessagesLocked();
  }

  bool retry_message = false;
  stub = stubs_.get(m->message.routing_id());
  if (stub) {
    if (!stub->IsScheduled()) {
      retry_message = true;
    }
    if (stub->IsPreempted()) {
      retry_message = true;
      message_queue_->ScheduleHandleMessage();
    }
  }

  if (retry_message) {
    base::AutoLock auto_lock(message_queue_->channel_messages_lock_);
    if (m->order_number == kOutOfOrderNumber)
      message_queue_->out_of_order_messages_.push_front(m);
    else
      message_queue_->channel_messages_.push_front(m);
    return;
  } else if (has_more_messages) {
    message_queue_->ScheduleHandleMessage();
  }

  scoped_ptr<GpuChannelMessage> scoped_message(m);
  const uint32_t order_number = m->order_number;
  const int32_t routing_id = m->message.routing_id();

  // TODO(dyen): Temporary handling of old sync points.
  // This must ensure that the sync point will be retired. Normally we'll
  // find the stub based on the routing ID, and associate the sync point
  // with it, but if that fails for any reason (channel or stub already
  // deleted, invalid routing id), we need to retire the sync point
  // immediately.
  if (m->message.type() == GpuCommandBufferMsg_InsertSyncPoint::ID) {
    const bool retire = m->retire_sync_point;
    const uint32_t sync_point = m->sync_point_number;
    if (stub) {
      stub->AddSyncPoint(sync_point);
      if (retire) {
        m->message =
            GpuCommandBufferMsg_RetireSyncPoint(routing_id, sync_point);
      }
    } else {
      current_order_num_ = order_number;
      gpu_channel_manager_->sync_point_manager()->RetireSyncPoint(sync_point);
      MessageProcessed(order_number);
      return;
    }
  }

  IPC::Message* message = &m->message;
  bool message_processed = true;

  DVLOG(1) << "received message @" << message << " on channel @" << this
           << " with type " << message->type();

  if (order_number != kOutOfOrderNumber) {
    // Make sure this is a valid unprocessed order number.
    DCHECK(order_number <= GetUnprocessedOrderNum() &&
           order_number >= GetProcessedOrderNum());

    current_order_num_ = order_number;
  }
  bool result = false;
  if (routing_id == MSG_ROUTING_CONTROL)
    result = OnControlMessageReceived(*message);
  else
    result = router_.RouteMessage(*message);

  if (!result) {
    // Respond to sync messages even if router failed to route.
    if (message->is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&*message);
      reply->set_reply_error();
      Send(reply);
    }
  } else {
    // If the command buffer becomes unscheduled as a result of handling the
    // message but still has more commands to process, synthesize an IPC
    // message to flush that command buffer.
    if (stub) {
      if (stub->HasUnprocessedCommands()) {
        message_queue_->PushUnfinishedMessage(
            order_number, GpuCommandBufferMsg_Rescheduled(stub->route_id()));
        message_processed = false;
      }
    }
  }
  if (message_processed)
    MessageProcessed(order_number);
}

void GpuChannel::OnCreateOffscreenCommandBuffer(
    const gfx::Size& size,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id,
    bool* succeeded) {
  TRACE_EVENT1("gpu", "GpuChannel::OnCreateOffscreenCommandBuffer", "route_id",
               route_id);

  int32 share_group_id = init_params.share_group_id;
  GpuCommandBufferStub* share_group = stubs_.get(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE) {
    *succeeded = false;
    return;
  }

  int32 stream_id = init_params.stream_id;
  GpuStreamPriority stream_priority = init_params.stream_priority;

  if (share_group && stream_id != share_group->stream_id()) {
    *succeeded = false;
    return;
  }

  if (!allow_real_time_streams_ &&
      stream_priority == GpuStreamPriority::REAL_TIME) {
    *succeeded = false;
    return;
  }

  auto stream_it = streams_.find(stream_id);
  if (stream_it != streams_.end() &&
      stream_priority != GpuStreamPriority::INHERIT &&
      stream_priority != stream_it->second.priority()) {
    *succeeded = false;
    return;
  }

  scoped_ptr<GpuCommandBufferStub> stub(new GpuCommandBufferStub(
      this, task_runner_.get(), share_group, gfx::GLSurfaceHandle(),
      mailbox_manager_.get(), subscription_ref_set_.get(),
      pending_valuebuffer_state_.get(), size, disallowed_features_,
      init_params.attribs, init_params.gpu_preference, false,
      init_params.stream_id, route_id, 0, watchdog_, software_,
      init_params.active_url));

  if (preempted_flag_.get())
    stub->SetPreemptByFlag(preempted_flag_);

  if (!router_.AddRoute(route_id, stub.get())) {
    DLOG(ERROR) << "GpuChannel::OnCreateOffscreenCommandBuffer(): "
                   "failed to add route";
    *succeeded = false;
    return;
  }

  if (stream_it != streams_.end()) {
    stream_it->second.AddRoute(route_id);
  } else {
    StreamState stream(stream_id, stream_priority);
    stream.AddRoute(route_id);
    streams_.insert(std::make_pair(stream_id, stream));
  }

  stubs_.set(route_id, stub.Pass());
  *succeeded = true;
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  scoped_ptr<GpuCommandBufferStub> stub = stubs_.take_and_erase(route_id);

  if (!stub)
    return;

  router_.RemoveRoute(route_id);

  int32 stream_id = stub->stream_id();
  auto stream_it = streams_.find(stream_id);
  DCHECK(stream_it != streams_.end());
  stream_it->second.RemoveRoute(route_id);
  if (!stream_it->second.HasRoutes())
    streams_.erase(stream_it);

  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the GpuChannel here.
  if (!stub->IsScheduled()) {
    // This stub won't get a chance to reschedule, so update the count now.
    StubSchedulingChanged(true);
  }
}

void GpuChannel::OnCreateJpegDecoder(int32 route_id, IPC::Message* reply_msg) {
  if (!jpeg_decoder_) {
    jpeg_decoder_.reset(new GpuJpegDecodeAccelerator(this, io_task_runner_));
  }
  jpeg_decoder_->AddClient(route_id, reply_msg);
}

void GpuChannel::MessageProcessed(uint32_t order_number) {
  if (order_number != kOutOfOrderNumber) {
    DCHECK(current_order_num_ == order_number);
    DCHECK(processed_order_num_ < order_number);
    processed_order_num_ = order_number;
  }
  if (preempting_flag_.get()) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GpuChannelMessageFilter::OnMessageProcessed, filter_));
  }
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->Send(
      new GpuHostMsg_CacheShader(client_id_, key, shader));
}

void GpuChannel::AddFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::AddChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

void GpuChannel::RemoveFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::RemoveChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

uint64 GpuChannel::GetMemoryUsage() {
  // Collect the unique memory trackers in use by the |stubs_|.
  std::set<gpu::gles2::MemoryTracker*> unique_memory_trackers;
  for (auto& kv : stubs_)
    unique_memory_trackers.insert(kv.second->GetMemoryTracker());

  // Sum the memory usage for all unique memory trackers.
  uint64 size = 0;
  for (auto* tracker : unique_memory_trackers) {
    size += gpu_channel_manager()->gpu_memory_manager()->GetTrackerMemoryUsage(
        tracker);
  }

  return size;
}

scoped_refptr<gfx::GLImage> GpuChannel::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32 internalformat) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      scoped_refptr<gfx::GLImageSharedMemory> image(
          new gfx::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle, format))
        return scoped_refptr<gfx::GLImage>();

      return image;
    }
    default: {
      GpuChannelManager* manager = gpu_channel_manager();
      if (!manager->gpu_memory_buffer_factory())
        return scoped_refptr<gfx::GLImage>();

      return manager->gpu_memory_buffer_factory()
          ->AsImageFactory()
          ->CreateImageForGpuMemoryBuffer(handle,
                                          size,
                                          format,
                                          internalformat,
                                          client_id_);
    }
  }
}

void GpuChannel::HandleUpdateValueState(
    unsigned int target, const gpu::ValueState& state) {
  pending_valuebuffer_state_->UpdateState(target, state);
}

uint32_t GpuChannel::GetUnprocessedOrderNum() const {
  return message_queue_->GetUnprocessedOrderNum();
}

}  // namespace content
