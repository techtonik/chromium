// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H

#include <jni.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/gcm_driver/gcm_driver.h"

namespace gcm {

// GCMDriver implementation for Android.
class GCMDriverAndroid : public GCMDriver {
 public:
  GCMDriverAndroid();
  virtual ~GCMDriverAndroid();

  // GCMDriver implementation:
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const RegisterCallback& callback) OVERRIDE;
  virtual void Unregister(const std::string& app_id,
                          const UnregisterCallback& callback) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    const SendCallback& callback) OVERRIDE;
  virtual GCMClient* GetGCMClientForTesting() const OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual bool IsGCMClientReady() const OVERRIDE;
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) OVERRIDE;
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) OVERRIDE;
  virtual std::string SignedInUserName() const OVERRIDE;

  // Register JNI methods.
  static bool RegisterBindings(JNIEnv* env);

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMDriverAndroid);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H
