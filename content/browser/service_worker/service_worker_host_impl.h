// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/service_worker_host.h"

namespace content {

class ServiceWorkerContextCore;

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code; call Send and the messages
// will be queued as needed and sent to the active service worker.
class ServiceWorkerHostImpl
    : public ServiceWorkerHost,
      public base::RefCountedThreadSafe<ServiceWorkerHostImpl> {
 public:
  ServiceWorkerHostImpl(const GURL& scope,
                        ServiceWorkerContextCore* context_core);

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerHostImpl>;

  virtual ~ServiceWorkerHostImpl() {}

  // The core context is only for use on the IO thread.
  ServiceWorkerContextCore* context();

  const GURL scope_;
  ServiceWorkerContextCore* context_core_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
