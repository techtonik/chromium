// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PERF_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PERF_PERF_PROVIDER_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "components/metrics/proto/sampled_profile.pb.h"

namespace metrics {

class WindowedIncognitoObserver;

// Provides access to ChromeOS perf data. perf aka "perf events" is a
// performance profiling infrastructure built into the linux kernel. For more
// information, see: https://perf.wiki.kernel.org/index.php/Main_Page.
class PerfProvider : public base::NonThreadSafe,
                     public chromeos::PowerManagerClient::Observer {
 public:
  PerfProvider();
  ~PerfProvider() override;

  // Stores collected perf data protobufs in |sampled_profiles|. Clears all the
  // stored profile data. Returns true if it wrote to |sampled_profiles|.
  bool GetSampledProfiles(std::vector<SampledProfile>* sampled_profiles);

 protected:
  typedef int64_t TimeDeltaInternalType;

  class CollectionParams {
   public:
    class TriggerParams {
     public:
      TriggerParams(int64_t sampling_factor,
                    base::TimeDelta max_collection_delay);

      int64_t sampling_factor() const { return sampling_factor_; }
      void set_sampling_factor(int64_t factor) { sampling_factor_ = factor; }

      base::TimeDelta max_collection_delay() const {
        return base::TimeDelta::FromInternalValue(max_collection_delay_);
      }
      void set_max_collection_delay(base::TimeDelta delay) {
        max_collection_delay_ = delay.ToInternalValue();
      }

     private:
      TriggerParams() = default;  // POD

      // Limit the number of profiles collected.
      int64_t sampling_factor_;

      // Add a random delay before collecting after the trigger.
      // The delay should be randomly selected between 0 and this value.
      TimeDeltaInternalType max_collection_delay_;
    };

    CollectionParams(base::TimeDelta collection_duration,
                     base::TimeDelta periodic_interval,
                     TriggerParams resume_from_suspend,
                     TriggerParams restore_session);

    base::TimeDelta collection_duration() const {
      return base::TimeDelta::FromInternalValue(collection_duration_);
    }
    void set_collection_duration(base::TimeDelta duration) {
      collection_duration_ = duration.ToInternalValue();
    }

    base::TimeDelta periodic_interval() const {
      return base::TimeDelta::FromInternalValue(periodic_interval_);
    }
    void set_periodic_interval(base::TimeDelta interval) {
      periodic_interval_ = interval.ToInternalValue();
    }

    const TriggerParams& resume_from_suspend() const {
      return resume_from_suspend_;
    }
    TriggerParams* mutable_resume_from_suspend() {
      return &resume_from_suspend_;
    }
    const TriggerParams& restore_session() const {
      return restore_session_;
    }
    TriggerParams* mutable_restore_session() {
      return &restore_session_;
    }

   private:
    CollectionParams() = default;  // POD

    // Time perf is run for.
    TimeDeltaInternalType collection_duration_;

    // For PERIODIC_COLLECTION, partition time since login into successive
    // intervals of this duration. In each interval, a random time is picked to
    // collect a profile.
    TimeDeltaInternalType periodic_interval_;

    // Parameters for RESUME_FROM_SUSPEND and RESTORE_SESSION collections:
    TriggerParams resume_from_suspend_;
    TriggerParams restore_session_;
  };

  // Parses a PerfDataProto from serialized data |perf_data|, if it exists.
  // Parses a PerfStatProto from serialized data |perf_stat|, if it exists.
  // Only one of these may contain data. If both |perf_data| and |perf_stat|
  // contain data, it is counted as an error and neither is parsed.
  // |incognito_observer| indicates whether an incognito window had been opened
  // during the profile collection period. If there was an incognito window,
  // discard the incoming data.
  // |trigger_event| is the cause of the perf data collection.
  // |result| is the return value of running perf/quipper. It is 0 if successful
  // and nonzero if not successful.
  void ParseOutputProtoIfValid(
      scoped_ptr<WindowedIncognitoObserver> incognito_observer,
      scoped_ptr<SampledProfile> sampled_profile,
      int result,
      const std::vector<uint8>& perf_data,
      const std::vector<uint8>& perf_stat);

 private:
  static const CollectionParams kDefaultParameters;

  // Class that listens for changes to the login state. When a normal user logs
  // in, it updates PerfProvider to start collecting data.
  class LoginObserver : public chromeos::LoginState::Observer {
   public:
    explicit LoginObserver(PerfProvider* perf_provider);

    // Called when either the login state or the logged in user type changes.
    // Activates |perf_provider_| to start collecting.
    void LoggedInStateChanged() override;

   private:
    // Points to a PerfProvider instance that can be turned on or off based on
    // the login state.
    PerfProvider* perf_provider_;
  };

  // Called when a suspend finishes. This is either a successful suspend
  // followed by a resume, or a suspend that was canceled. Inherited from
  // PowerManagerClient::Observer.
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Turns on perf collection. Resets the timer that's used to schedule
  // collections.
  void OnUserLoggedIn();

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  // Turns off perf collection. Does not delete any data that was already
  // collected and stored in |cached_perf_data_|.
  void Deactivate();

  // Selects a random time in the upcoming profiling interval that begins at
  // |next_profiling_interval_start_|. Schedules |timer_| to invoke
  // DoPeriodicCollection() when that time comes.
  void ScheduleIntervalCollection();

  // Collects perf data for a given |trigger_event|. Calls perf via the ChromeOS
  // debug daemon's dbus interface.
  void CollectIfNecessary(scoped_ptr<SampledProfile> sampled_profile);

  // Collects perf data on a repeating basis by calling CollectIfNecessary() and
  // reschedules it to be collected again.
  void DoPeriodicCollection();

  // Collects perf data after a resume. |sleep_duration| is the duration the
  // system was suspended before resuming. |time_after_resume_ms| is how long
  // ago the system resumed.
  void CollectPerfDataAfterResume(const base::TimeDelta& sleep_duration,
                                  const base::TimeDelta& time_after_resume);

  // Collects perf data after a session restore. |time_after_restore| is how
  // long ago the session restore started. |num_tabs_restored| is the total
  // number of tabs being restored.
  void CollectPerfDataAfterSessionRestore(
      const base::TimeDelta& time_after_restore,
      int num_tabs_restored);

  // Parameters controlling how profiles are collected.
  CollectionParams collection_params_;

  // Vector of SampledProfile protobufs containing perf profiles.
  std::vector<SampledProfile> cached_perf_data_;

  // For scheduling collection of perf data.
  base::OneShotTimer timer_;

  // For detecting when changes to the login state.
  LoginObserver login_observer_;

  // Record of the last login time.
  base::TimeTicks login_time_;

  // Record of the start of the upcoming profiling interval.
  base::TimeTicks next_profiling_interval_start_;

  // Tracks the last time a session restore was collected.
  base::TimeTicks last_session_restore_collection_time_;

  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription object's destructor will automatically unregister the
  // callback in SessionRestore, so that the callback list does not contain any
  // obsolete callbacks.
  SessionRestore::CallbackSubscription
      on_session_restored_callback_subscription_;

  // To pass around the "this" pointer across threads safely.
  base::WeakPtrFactory<PerfProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerfProvider);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PERF_PROVIDER_CHROMEOS_H_
