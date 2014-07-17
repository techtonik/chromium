// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_host_impl.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_host_client.h"
#include "ipc/ipc_message.h"

namespace {

using content::BrowserThread;
using content::SERVICE_WORKER_OK;
using content::ServiceWorkerContextWrapper;
using content::ServiceWorkerRegistration;
using content::ServiceWorkerStatusCode;
using content::ServiceWorkerVersion;

void OnRegistrationFoundSendMessage(
    IPC::Message* message,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  fprintf(stderr, "%s:%s:%d \n", __FILE__, __FUNCTION__, __LINE__);
  if (status != SERVICE_WORKER_OK) {  // || !registration->active_version()) {
    //
    //
    // TODO(scheib) Inform the Host client that we failed to send?
    //
    //
    fprintf(stderr,
            "%s:%s:%d returning early %d, ok:%d, active %d\n",
            __FILE__,
            __FUNCTION__,
            __LINE__,
            status,
            status == SERVICE_WORKER_OK,
            !!registration->active_version());
    return;
  }
  CHECK(message);  // Check pointer before taking a reference to it.
  //
  //
  // TODO(scheib) Inform the Host client that we failed to send (via callback)?
  //
  //
  fprintf(stderr,
          "%s:%s:%d ok, calling registration->active_version()->SendMessage\n",
          __FILE__,
          __FUNCTION__,
          __LINE__);
  registration->active_version()->SendMessage(
      *message, ServiceWorkerVersion::StatusCallback());
}

void SendOnIO(scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
              const GURL scope,
              IPC::Message* message) {
  fprintf(stderr, "%s:%s:%d \n", __FILE__, __FUNCTION__, __LINE__);
  //
  //
  // TODO: Optimize by keeping a reference to Registration.
  //
  //
  context_wrapper->context()->storage()->FindRegistrationForPattern(
      scope, base::Bind(&OnRegistrationFoundSendMessage, message));
}

}  // namespace

namespace content {

ServiceWorkerHostImpl::ServiceWorkerHostImpl(
    const GURL& scope,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerHostClient* client)
    : scope_(scope),
      context_wrapper_(context_wrapper),
      ui_thread_(client),
      io_thread_(registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Take a reference to to the object instance now. It is released in
  // DisconnectAndDeleteOnIO(). See class comments Note on Lifetime.
  AddRef();

  io_thread_.registration->AddListener(this);
}

const GURL& ServiceWorkerHostImpl::scope() {
  return scope_;
}

const GURL& ServiceWorkerHostImpl::script() {
  return script_;
}

bool ServiceWorkerHostImpl::HasInstalled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ui_thread_.has_installed;
}

bool ServiceWorkerHostImpl::HasActivated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ui_thread_.has_activated;
}

bool ServiceWorkerHostImpl::Send(IPC::Message* message) {
  fprintf(stderr, "%s:%s:%d \n", __FILE__, __FUNCTION__, __LINE__);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(SendOnIO, context_wrapper_, scope_, message));
  return true;
}

void ServiceWorkerHostImpl::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ServiceWorkerHostImpl::OnVersionAttributesChangedOnUI,
                 this,
                 changed_mask,
                 info));
}

void ServiceWorkerHostImpl::DisconnectClientAndDeleteOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ui_thread_.client = NULL;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerHostImpl::DisconnectAndDeleteOnIO, this));
}

ServiceWorkerHostImpl::~ServiceWorkerHostImpl() {
  //
  //
  // TODO: Disconnect client from wherever we've registered it
  //
  //
}

void ServiceWorkerHostImpl::DisconnectAndDeleteOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  io_thread_.registration->RemoveListener(this);

  // Release reference from constructor. See class comments Note on Lifetime.
  Release();
  // We are likely be destroyed here! Callbacks in message queues may still
  // hold references.
}

void ServiceWorkerHostImpl::OnVersionAttributesChangedOnUI(
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ui_thread_.client)
    return;

  if (!ui_thread_.has_installed &&
      info.active_version.status == ServiceWorkerVersion::INSTALLED) {
    ui_thread_.has_installed = true;
    ui_thread_.client->OnInstalled();
  }

  if (!ui_thread_.has_activated &&
      info.active_version.status == ServiceWorkerVersion::ACTIVATED) {
    ui_thread_.has_activated = true;
    ui_thread_.client->OnActivated();
  }
}

// ServiceWorkerHostImpl::UIThreadMembers

ServiceWorkerHostImpl::UIThreadMembers::UIThreadMembers(
    ServiceWorkerHostClient* client)
    : client(client), has_installed(false), has_activated(false) {
}

ServiceWorkerHostImpl::UIThreadMembers::~UIThreadMembers() {
}

// ServiceWorkerHostImpl::IOThreadMembers

ServiceWorkerHostImpl::IOThreadMembers::IOThreadMembers(
    const scoped_refptr<ServiceWorkerRegistration>& registration)
    : registration(registration) {
}

ServiceWorkerHostImpl::IOThreadMembers::~IOThreadMembers() {
}

}  // namespace content
