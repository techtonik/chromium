// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_host_impl.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
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

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : observer_list_(
          new ObserverListThreadSafe<ServiceWorkerContextObserver>()),
      browser_context_(browser_context) {
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(
              BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  InitInternal(user_data_directory, database_task_runner, quota_manager_proxy);
}

void ServiceWorkerContextWrapper::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    browser_context_ = NULL;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Shutdown, this));
    return;
  }
  // Breaks the reference cycle through ServiceWorkerProcessManager.
  context_core_.reset();
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const Scope& scope,
    const GURL& script_url,
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
                   client,
                   callback));
    return;
  }

  context()->RegisterServiceWorker(
      scope,
      script_url,
      -1,
      NULL /* provider_host */,
      base::Bind(&ServiceWorkerContextWrapper::FinishRegistrationOnIO,
                 this,
                 scope,
                 client,
                 callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& scope,
    const ResultCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   scope,
                   callback));
    return;
  }

  context()->UnregisterServiceWorker(
      scope, base::Bind(&PostResultToUIFromStatusOnIO, callback));
}

void ServiceWorkerContextWrapper::GetServiceWorkerHost(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ServiceWorkerHostCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::GetServiceWorkerHost,
                   this,
                   scope,
                   client,
                   callback));
    return;
  }
  callback.Run(scoped_ptr<ServiceWorkerHost>(
      new ServiceWorkerHostImpl()));
//      new ServiceWorkerHostImpl(scope, context(), client)));
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::InitForTesting(
    const base::FilePath& user_data_directory,
    base::SequencedTaskRunner* database_task_runner,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  InitInternal(user_data_directory, database_task_runner, quota_manager_proxy);
}

void ServiceWorkerContextWrapper::InitInternal(
    const base::FilePath& user_data_directory,
    base::SequencedTaskRunner* database_task_runner,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::InitInternal,
                   this,
                   user_data_directory,
                   make_scoped_refptr(database_task_runner),
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(new ServiceWorkerContextCore(
      user_data_directory,
      database_task_runner,
      quota_manager_proxy,
      observer_list_,
      make_scoped_ptr(new ServiceWorkerProcessManager(this))));
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
