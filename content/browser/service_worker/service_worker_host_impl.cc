// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_host_impl.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"

namespace {

using content::BrowserThread;
using content::SERVICE_WORKER_OK;
using content::ServiceWorkerRegistration;
using content::ServiceWorkerStatusCode;
using content::ServiceWorkerVersion;

void SendMessageAfterFind(
    IPC::Message* message,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status != SERVICE_WORKER_OK || !registration->active_version() ||
      !message) {
    // TODO(scheib) Inform the Host client that we failed to send?
    return;
  }
  // TODO(scheib) Inform the Host client that we failed to send (via callback)?
  registration->active_version()->SendMessage(
      *message, ServiceWorkerVersion::StatusCallback());
}

}  // namespace

namespace content {

ServiceWorkerHostImpl::ServiceWorkerHostImpl(
    const GURL& scope,
    ServiceWorkerContextCore* context_core)
    : scope_(scope), context_core_(context_core) {
}

bool ServiceWorkerHostImpl::Send(IPC::Message* message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(&ServiceWorkerHostImpl::Send), this, message));
  } else {
    context()->storage()->FindRegistrationForPattern(
        scope_, base::Bind(&SendMessageAfterFind, message));
  }
  return true;
}

ServiceWorkerContextCore* ServiceWorkerHostImpl::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_;
}

}  // namespace content
