// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"

namespace content {

class ServiceWorkerHost;

// Interface for clients of ServiceWorkerHost listening to messages from service
// worker version farthest along the install flow, typically the current active
// version.
//
// IPC::Listener OnMessageReceived is called for each ServiceWorkerHostClient
// that is known in first-discovered first-called order. When OnMessageReceived
// returns true no additional instances will have OnMessageReceived called.
//
// A ServiceWorkerHostClient object disconnects from ServiceWorkerHost
// automatically at client destruction. by calling
// ServiceWorkerHost::DisconnectServiceWorkerHostClient.
class CONTENT_EXPORT ServiceWorkerHostClient : public IPC::Listener {
 public:
  ServiceWorkerHostClient() : service_worker_host_(NULL) {}

  ServiceWorkerHost* service_worker_host() { return service_worker_host_; }
  void set_service_worker_host(ServiceWorkerHost* service_worker_host) {
    service_worker_host_ = service_worker_host;
  }

  // Called when a service worker has been installed, activated:
  virtual void OnInstalled() {}
  virtual void OnActivated() {}

 protected:
  virtual ~ServiceWorkerHostClient();

  ServiceWorkerHost* service_worker_host_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_HOST_CLIENT_H_
