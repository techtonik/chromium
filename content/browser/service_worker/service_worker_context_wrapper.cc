// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper() {
}

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
  context_core_.reset(
      new ServiceWorkerContextCore(
          user_data_directory, quota_manager_proxy));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return context_core_.get();
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    int source_process_id,
    const RegistrationCallback& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorkerOnIO,
                 this,
                 pattern,
                 script_url,
                 source_process_id,
                 continuation));
}

void ServiceWorkerContextWrapper::RegisterServiceWorkerOnIO(
    const GURL& pattern,
    const GURL& script_url,
    int source_process_id,
    const RegistrationCallback& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context()->RegisterServiceWorker(
      pattern,
      script_url,
      source_process_id,
      base::Bind(&ServiceWorkerContextWrapper::FinishRegistrationOnIO,
                 this,
                 continuation));
}

void ServiceWorkerContextWrapper::FinishRegistrationOnIO(
    const RegistrationCallback& continuation,
    ServiceWorkerStatusCode status,
    int64 registration_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::Bind(&BrowserThread::PostTask,
             BrowserThread::UI,
             FROM_HERE,
             base::Bind(continuation, status));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& pattern,
    int source_process_id,
    const UnregistrationCallback& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorkerOnIO,
                 this,
                 pattern,
                 source_process_id,
                 continuation));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorkerOnIO(
    const GURL& pattern,
    int source_process_id,
    const UnregistrationCallback& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context()->UnregisterServiceWorker(
      pattern,
      source_process_id,
      base::Bind(&ServiceWorkerContextWrapper::FinishUnregistrationOnIO,
                 this,
                 continuation));
}
void ServiceWorkerContextWrapper::FinishUnregistrationOnIO(
    const RegistrationCallback& continuation,
    ServiceWorkerStatusCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::Bind(&BrowserThread::PostTask,
             BrowserThread::UI,
             FROM_HERE,
             base::Bind(continuation, status));
}

}  // namespace content
