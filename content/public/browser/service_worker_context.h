// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "content/public/common/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

// Represents the per-StoragePartition ServiceWorker data.  Must be used from
// the UI thread.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  typedef GURL Scope;

  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;

  virtual void RegisterServiceWorker(const Scope& pattern,
                                     const GURL& script_url,
                                     int source_process_id,
                                     const StatusCallback& callback) = 0;

  virtual void UnregisterServiceWorker(const GURL& pattern,
                                       int source_process_id,
                                       const StatusCallback& callback) = 0;

  // TODO(jyasskin): Provide a way to SendMessage to a Scope.

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
