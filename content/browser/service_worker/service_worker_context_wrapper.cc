// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
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
    BrowserThread::PostTask(
        BrowserThread::UI,
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
    const WorkerCallback& continuation) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                   this,
                   scope,
                   script_url,
                   source_process_id,
                   continuation));
    return;
  }

  context()->RegisterServiceWorker(
      scope,
      script_url,
      source_process_id,
      NULL /* provider_host */,
      base::Bind(&ServiceWorkerContextWrapper::FinishRegistrationOnIO, this, scope, continuation));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const Scope& scope,
    int source_process_id,
    const ResultCallback& continuation) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   scope,
                   source_process_id,
                   continuation));
    return;
  }

  context()->UnregisterServiceWorker(
      scope,
      source_process_id,
      NULL /* provider_host */,
      base::Bind(&PostResultToUIFromStatusOnIO, continuation));
}

void ServiceWorkerContextWrapper::GetServiceWorkerHost(
    const Scope& scope,
    const WorkerCallback& callback) {
  // 
  // TODO(scheib) Still need to implement this for this patch.
  //
  NOTIMPLEMENTED();
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
    const WorkerCallback& callback,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == SERVICE_WORKER_OK) {
    ServiceWorkerContextWrapper::GetServiceWorkerHost(scope, callback);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback, base::WeakPtr<ServiceWorkerHost>()));
  }
}

}  // namespace content
