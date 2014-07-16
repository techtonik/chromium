// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_host_client.h"

#include "content/browser/service_worker/service_worker_host_impl.h"

namespace content {

ServiceWorkerHostClient::ServiceWorkerHostClient() {
}

ServiceWorkerHost* ServiceWorkerHostClient::service_worker_host() {
  return service_worker_host_.get();
}

void ServiceWorkerHostClient::set_service_worker_host(
    const scoped_refptr<ServiceWorkerHostImpl>& service_worker_host) {
  service_worker_host_ = service_worker_host;
}

ServiceWorkerHostClient::~ServiceWorkerHostClient() {
  if (service_worker_host_.get())
    service_worker_host_->DisconnectServiceWorkerHostClient();
}

}  // namespace content
