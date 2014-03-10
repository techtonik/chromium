// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

using base::Callback;
using base::Closure;
using base::WeakPtr;
using content::BrowserContext;
using content::BrowserThread;
using content::ServiceWorkerStatusCode;

static const char* CurrentBrowserThreadName() {
  BrowserThread::ID id;
  if (BrowserThread::GetCurrentThreadIdentifier(&id)) {
    switch (id) {
      case BrowserThread::UI:
        return "UI";
      case BrowserThread::DB:
        return "DB";
      case BrowserThread::FILE:
        return "FILE";
      case BrowserThread::FILE_USER_BLOCKING:
        return "FILE_USER_BLOCKING";
      case BrowserThread::PROCESS_LAUNCHER:
        return "PROCESS_LAUNCHER";
      case BrowserThread::CACHE:
        return "CACHE";
      case BrowserThread::IO:
        return "IO";
      default:
        break;
    }
  }
  return "Unknown";
}

static void DCheckCurrentlyOn(BrowserThread::ID thread) {
  DCHECK(BrowserThread::CurrentlyOn(thread)) << CurrentBrowserThreadName();
}

ServiceWorkerManager::State::State() {}
ServiceWorkerManager::State::~State() {}

ServiceWorkerManager::ServiceWorkerManager(BrowserContext* context)
    : context_(context), weak_this_factory_(this) {}
ServiceWorkerManager::~ServiceWorkerManager() {}

ServiceWorkerManager* ServiceWorkerManager::Get(
    content::BrowserContext* context) {
  return ServiceWorkerManagerFactory::GetForBrowserContext(context);
}

content::StoragePartition* ServiceWorkerManager::GetStoragePartition(
    const Extension* ext) const {
  return content::BrowserContext::GetStoragePartitionForSite(
      context_, Extension::GetBaseURLFromExtensionId(ext->id()));
}

scoped_refptr<content::ServiceWorkerContextWrapper>
ServiceWorkerManager::GetSWContext(const Extension* ext) const {
  return GetStoragePartition(ext)->GetServiceWorkerContext();
}

static void FinishRegister(
    const Callback<void(ServiceWorkerStatusCode)>& continuation,
    content::ServiceWorkerStatusCode status,
    int64 registration_id) {
  DCheckCurrentlyOn(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(continuation, status));
}

static void DoRegister(
    const scoped_refptr<content::ServiceWorkerContextWrapper>&
        service_worker_context,
    const ExtensionId& extension_id,
    const GURL& service_worker_script,
    const Callback<void(ServiceWorkerStatusCode)>& continuation) {
  DCheckCurrentlyOn(BrowserThread::IO);
  service_worker_context->context()->RegisterServiceWorker(
      Extension::GetBaseURLFromExtensionId(extension_id).Resolve("/*"),
      service_worker_script,
      -1,
      base::Bind(&FinishRegister, continuation));
}

// alecflett says that if we send a series of RegisterServiceWorker and
// UnregisterServiceWorker calls to a ServiceWorkerContextCore, we're guaranteed
// that the callbacks come back in the same order, and that the last one will be
// the final state.
void ServiceWorkerManager::RegisterExtension(const Extension* extension) {
  DCheckCurrentlyOn(BrowserThread::UI);
  CHECK(BackgroundInfo::HasServiceWorker(extension));
  State& ext_state = states_[extension->id()];
  if (ext_state.registration == REGISTERING ||
      ext_state.registration == REGISTERED)
    return;
  ext_state.registration = REGISTERING;
  ++ext_state.outstanding_state_changes;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DoRegister,
                 GetSWContext(extension),
                 extension->id(),
                 extension->GetResourceURL(
                     BackgroundInfo::GetServiceWorkerScript(extension)),
                 base::Bind(&ServiceWorkerManager::FinishRegistration,
                            WeakThis(),
                            extension->id())));
}

void ServiceWorkerManager::FinishRegistration(const ExtensionId& extension_id,
                                              ServiceWorkerStatusCode result) {
  DCheckCurrentlyOn(BrowserThread::UI);
  State& ext_state = states_[extension_id];
  --ext_state.outstanding_state_changes;
  DCHECK_GE(ext_state.outstanding_state_changes, 0);
  if (ext_state.outstanding_state_changes > 0)
    return;

  DCHECK_EQ(ext_state.registration, REGISTERING);
  std::vector<Closure> to_run;
  switch (result) {
    case content::SERVICE_WORKER_OK:
      ext_state.registration = REGISTERED;
      to_run.swap(ext_state.registration_succeeded);
      ext_state.registration_failed.clear();
      break;
    default:
      LOG(ERROR) << "Service Worker Registration failed for extension "
                 << extension_id << ": "
                 << content::ServiceWorkerStatusToString(result);
      to_run.swap(ext_state.registration_failed);
      states_.erase(extension_id);
      break;
  }

  for (size_t i = 0; i < to_run.size(); ++i) {
    to_run[i].Run();
  }
}

static void FinishUnregister(
    const Callback<void(ServiceWorkerStatusCode)>& continuation,
    content::ServiceWorkerStatusCode status) {
  DCheckCurrentlyOn(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RunCallback1<ServiceWorkerStatusCode>, continuation, status));
}

static void DoUnregister(
    const scoped_refptr<content::ServiceWorkerContextWrapper>&
        service_worker_context,
    const ExtensionId& extension_id,
    const Callback<void(ServiceWorkerStatusCode)>& continuation) {
  DCheckCurrentlyOn(BrowserThread::IO);
  service_worker_context->context()->UnregisterServiceWorker(
      Extension::GetBaseURLFromExtensionId(extension_id).Resolve("/*"),
      -1,
      base::Bind(&FinishUnregister, continuation));
}

void ServiceWorkerManager::UnregisterExtension(const Extension* extension) {
  DCheckCurrentlyOn(BrowserThread::UI);
  CHECK(BackgroundInfo::HasServiceWorker(extension));

  base::hash_map<ExtensionId, State>::iterator it =
      states_.find(extension->id());
  if (it == states_.end()) {
    // Extension isn't registered.
    return;
  }
  State& ext_state = it->second;
  if (ext_state.registration == UNREGISTERING)
    return;

  ext_state.registration = UNREGISTERING;
  ++ext_state.outstanding_state_changes;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DoUnregister,
                 GetSWContext(extension),
                 extension->id(),
                 base::Bind(&ServiceWorkerManager::FinishUnregistration,
                            WeakThis(),
                            extension->id())));
}

void ServiceWorkerManager::FinishUnregistration(
    const ExtensionId& extension_id,
    ServiceWorkerStatusCode result) {
  DCheckCurrentlyOn(BrowserThread::UI);
  State& ext_state = states_[extension_id];
  --ext_state.outstanding_state_changes;
  DCHECK_GE(ext_state.outstanding_state_changes, 0);
  if (ext_state.outstanding_state_changes > 0)
    return;

  DCHECK_EQ(ext_state.registration, UNREGISTERING);
  std::vector<Closure> to_run;
  switch (result) {
    case content::SERVICE_WORKER_OK:
      to_run.swap(ext_state.unregistration_succeeded);
      states_.erase(extension_id);
      break;
    default:
      LOG(ERROR) << "Service Worker Unregistration failed for extension "
                 << extension_id << ": "
                 << content::ServiceWorkerStatusToString(result);
      ext_state.registration = REGISTERED;
      to_run.swap(ext_state.unregistration_failed);
      ext_state.unregistration_succeeded.clear();
      break;
  }

  for (size_t i = 0; i < to_run.size(); ++i) {
    to_run[i].Run();
  }
}

WeakPtr<ServiceWorkerManager> ServiceWorkerManager::WeakThis() {
  return weak_this_factory_.GetWeakPtr();
}

// ServiceWorkerManagerFactory

ServiceWorkerManager* ServiceWorkerManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ServiceWorkerManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ServiceWorkerManagerFactory* ServiceWorkerManagerFactory::GetInstance() {
  return Singleton<ServiceWorkerManagerFactory>::get();
}

void ServiceWorkerManagerFactory::SetInstanceForTesting(
    content::BrowserContext* context,
    ServiceWorkerManager* manager) {
  Associate(context, manager);
}

ServiceWorkerManagerFactory::ServiceWorkerManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ServiceWorkerManager",
          BrowserContextDependencyManager::GetInstance()) {}

ServiceWorkerManagerFactory::~ServiceWorkerManagerFactory() {}

BrowserContextKeyedService*
ServiceWorkerManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ServiceWorkerManager(context);
}

// TODO(jyasskin): Deal with incognito mode.
content::BrowserContext* ServiceWorkerManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
