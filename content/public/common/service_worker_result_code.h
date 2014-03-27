// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_WORKER_RESULT_CODE_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_WORKER_RESULT_CODE_H_

#include "content/common/content_export.h"

namespace content {

// Possible results from the public service worker API.
enum ServiceWorkerResultCode {
  // Operation succeeded.
  SERVICE_WORKER_SUCCEEDED,

  // The operation failed. (More specific error codes may be added in the
  // future.)
  SERVICE_WORKER_FAILED,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_WORKER_RESULT_CODE_H_
