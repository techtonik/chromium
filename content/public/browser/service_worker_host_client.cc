// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_host_client.h"

#include "content/browser/service_worker/service_worker_host_impl.h"

namespace content {

ServiceWorkerHostClient::~ServiceWorkerHostClient() {
  ServiceWorkerHostImpl* swh_impl =
      static_cast<ServiceWorkerHostImpl*>(service_worker_host_);
  if (swh_impl)
    swh_impl->DisconnectClientAndDeleteOnUI();
}

}  // namespace content
