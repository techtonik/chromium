// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_

#include "ipc/ipc_sender.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerHostImpl;

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code; call Send and the messages
// will be queued as needed and sent to the active service worker.
class ServiceWorkerHost : public IPC::Sender {
 private:
  friend ServiceWorkerHostImpl;
  ServiceWorkerHost() {};
  virtual ~ServiceWorkerHost() {};
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
