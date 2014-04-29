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
using content::ServiceWorkerContextCore;
using content::ServiceWorkerRegistration;
using content::ServiceWorkerStatusCode;
using content::ServiceWorkerVersion;

void OnRegistrationFoundSendMessage(
    IPC::Message* message,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status != SERVICE_WORKER_OK || !registration->active_version()) {
    // TODO(scheib) Inform the Host client that we failed to send?
    return;
  }
  CHECK(message);  // Check pointer before taking a reference to it.
  // TODO(scheib) Inform the Host client that we failed to send (via callback)?
  registration->active_version()->SendMessage(
      *message, ServiceWorkerVersion::StatusCallback());
}

void SendOnIO(ServiceWorkerContextCore* context,
              const GURL scope,
              IPC::Message* message) {
  context->storage()->FindRegistrationForPattern(
      scope, base::Bind(&OnRegistrationFoundSendMessage, message));
}

}  // namespace

namespace content {

ServiceWorkerHostImpl::ServiceWorkerHostImpl(
    const GURL& scope,
    ServiceWorkerContextCore* context_core,
    ServiceWorkerHostClient* client)
    : scope_(scope), context_core_(context_core), client_(client) {
  NOTIMPLEMENTED();
  //
  //
  // TODO: Register the client and implement interface.
  //
  //
}

const GURL& ServiceWorkerHostImpl::scope() {
  return scope_;
}

const GURL& ServiceWorkerHostImpl::script() {
  return script_;
}

bool ServiceWorkerHostImpl::HasActiveVersion() {
  NOTIMPLEMENTED();  // TODO
  return false;
}

bool ServiceWorkerHostImpl::Send(IPC::Message* message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(SendOnIO, context(), scope_, message));
  return true;
}

ServiceWorkerContextCore* ServiceWorkerHostImpl::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_;
}

}  // namespace content
