// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_host_impl.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_host.h"
#include "content/public/browser/service_worker_host_client.h"
#include "ipc/ipc_message.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace {

using content::BrowserThread;
using content::SERVICE_WORKER_OK;
using content::ServiceWorkerContext;
using content::ServiceWorkerRegistration;
using content::ServiceWorkerStatusCode;

void PostResultToUIFromStatusOnIO(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!callback.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, status == SERVICE_WORKER_OK));
  }
}

}  // namespace

namespace content {

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper()
    : observer_list_(
          new ObserverListThreadSafe<ServiceWorkerContextObserver>()) {}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Init, this,
                   user_data_directory,
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(new ServiceWorkerContextCore(
      user_data_directory, quota_manager_proxy, observer_list_));
}

void ServiceWorkerContextWrapper::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Shutdown, this));
    return;
  }
  context_core_.reset();
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const Scope& scope,
    const GURL& script_url,
    int source_process_id,
    ServiceWorkerHostClient* client,
    const ServiceWorkerHostCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                   this,
                   scope,
                   script_url,
                   source_process_id,
                   client,
                   callback));
    return;
  }

  context()->RegisterServiceWorker(
      scope,
      script_url,
      source_process_id,
      NULL /* provider_host */,
      base::Bind(&ServiceWorkerContextWrapper::FinishRegistrationOnIO,
                 this,
                 scope,
                 client,
                 callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const Scope& scope,
    int source_process_id,
    const ResultCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   scope,
                   source_process_id,
                   callback));
    return;
  }

  context()->UnregisterServiceWorker(
      scope,
      source_process_id,
      NULL /* provider_host */,
      base::Bind(&PostResultToUIFromStatusOnIO, callback));
}

void ServiceWorkerContextWrapper::GetServiceWorkerHost(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ServiceWorkerHostCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(scoped_ptr<ServiceWorkerHost>(
      new ServiceWorkerHostImpl(scope, context(), client)));
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::FinishRegistrationOnIO(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ServiceWorkerHostCallback& callback,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  //
  // TODO: Do something if (status != SERVICE_WORKER_OK).
  //
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::GetServiceWorkerHost,
                 this,
                 scope,
                 client,
                 callback));
}

}  // namespace content
