// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_histogram_manager.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/metrics/statistics_recorder.h"
#include "components/cronet/histogram_manager.h"

#include "jni/CronetHistogramManager_jni.h"

namespace cronet {

// Explicitly register static JNI functions.
bool CronetHistogramManagerRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void EnsureInitialized(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller) {
  base::StatisticsRecorder::Initialize();
}

static ScopedJavaLocalRef<jbyteArray> GetHistogramDeltas(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  std::vector<uint8> data;
  if (!HistogramManager::GetInstance()->GetDeltas(&data))
    return ScopedJavaLocalRef<jbyteArray>();
  return base::android::ToJavaByteArray(env, &data[0], data.size());
}

}  // namespace cronet
