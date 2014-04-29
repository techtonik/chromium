// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_

#include "ipc/ipc_listener.h"

namespace content {

// Interface for clients of ServiceWorkerHost listening to messages from service
// worker version farthest along the install flow, typically the current active
// version.
//
// IPC::Listener OnMessageReceived is called for each ServiceWorkerHostClient
// that is known in first-discovered first-called order. When OnMessageReceived
// returns true no additional instances will have OnMessageReceived called.
//
// A ServiceWorkerHostClient object is disconnected by deleting the associated
// ServiceWorkerHost.
class ServiceWorkerHostClient : public IPC::Listener {
  // When the service worker being listened to changes version (to a new one,
  // or to an unregistered state).
  virtual void OnVersionChanged() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_
