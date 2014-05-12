// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_sender.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerHostClient;
class ServiceWorkerHostImpl;

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code.
//
// Refcounted to make sense out of multiple calls to GetServiceWorkerHost and/or
// RegisterServiceWorker which return the same object. If an individual caller
// is no longer interested in listening, RemoveListner and drop ref.
class ServiceWorkerHost : public IPC::Sender {
 public:
  virtual ~ServiceWorkerHost() {};

// // Identifying attributes.
// virtual const GURL& scope() = 0;
// virtual const GURL& script() = 0;
//
// // True when a version is installed and activated.
// virtual bool HasActiveVersion() = 0;
//
// // IPC::Sender interface:
// // Sends a message to the version farthest along in in the install flow,
// // typically the current active version.
// // (Some messages may be dropped during version transitions.)
// // TODO: michaeln added above comment - until I get into implementation I'm
// // not certain why we aren't able to queue up messages and ensure delivery.
// //virtual bool Send(IPC::Message* msg) OVERRIDE;
//
 private:
  friend ServiceWorkerHostImpl;
  ServiceWorkerHost() {};
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
