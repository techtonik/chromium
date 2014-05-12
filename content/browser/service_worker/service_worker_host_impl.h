// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_

#include "content/public/browser/service_worker_host.h"

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class ServiceWorkerContextCore;

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code; call Send and the messages
// will be queued as needed and sent to the active service worker.
class CONTENT_EXPORT ServiceWorkerHostImpl : NON_EXPORTED_BASE(public ServiceWorkerHost) {
 public:
   ServiceWorkerHostImpl(const GURL& scope,
                         ServiceWorkerContextCore* context_core,
                         ServiceWorkerHostClient* client);

// // ServiceWorkerHost implementation:
// virtual const GURL& scope() OVERRIDE;
// virtual const GURL& script() OVERRIDE;
// virtual bool HasActiveVersion() OVERRIDE;
//
// // IPC::Sender implementation.
 virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  virtual ~ServiceWorkerHostImpl();
//
// // The core context is only for use on the IO thread.
// ServiceWorkerContextCore* context();
//
 const GURL scope_;
// const GURL script_;  // TODO: implement this existing.
 ServiceWorkerContextCore* context_core_;
 ServiceWorkerHostClient* client_;
//     ALLOW_UNUSED;  // TODO: remove macro once used
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
