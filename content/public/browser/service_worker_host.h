// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_

#include "ipc/ipc_sender.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerHostClient;

// Interface to communicate with service workers from the UI thread. Abstracts
// the lifetime and active version for calling code.
class ServiceWorkerHost : public IPC::Sender {
 public:
  // Identifying attributes.
  virtual const GURL& scope() = 0;
  virtual const GURL& script() = 0;

  // True when a version has been installed, activated:
  virtual bool HasActivated() = 0;
  virtual bool HasInstalled() = 0;

  // IPC::Sender interface:
  // Sends a message to the version farthest along in in the install flow,
  // typically the current active version.
  //
  // (Some messages may be dropped during version transitions.)
  // TODO: michaeln added above comment - until I get into implementation I'm
  // not certain why we aren't able to queue up messages and ensure delivery.
  //
  // TODO: Do we need to address each version of a service worker?
  // (registered, installing, installed but not active, active)
  virtual bool Send(IPC::Message* msg) OVERRIDE = 0;

 protected:
  ServiceWorkerHost() {}
  virtual ~ServiceWorkerHost() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHost);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_H_
