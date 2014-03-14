// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/extension_host.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

using base::Callback;
using base::Closure;
using base::WeakPtr;
using content::BrowserContext;
using content::BrowserThread;
using content::ServiceWorkerStatusCode;

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
    const ExtensionId& ext_id) const {
  return content::BrowserContext::GetStoragePartitionForSite(
      context_, Extension::GetBaseURLFromExtensionId(ext_id));
}

content::ServiceWorkerContext* ServiceWorkerManager::GetSWContext(
    const ExtensionId& ext_id) const {
  return GetStoragePartition(ext_id)->GetServiceWorkerContext();
}

// alecflett says that if we send a series of RegisterServiceWorker and
// UnregisterServiceWorker calls on the same scope to a
// ServiceWorkerContextCore, we're guaranteed that the callbacks come back in
// the same order, and that the last one will be the final state.
void ServiceWorkerManager::RegisterExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(BackgroundInfo::HasServiceWorker(extension));
  State& ext_state = states_[extension->id()];
  if (ext_state.registration == REGISTERING ||
      ext_state.registration == REGISTERED)
    return;
  ext_state.registration = REGISTERING;
  ++ext_state.outstanding_state_changes;
  const GURL service_worker_script = extension->GetResourceURL(
      BackgroundInfo::GetServiceWorkerScript(extension));
  // TODO(jyasskin): Create the extension process in a cleaner way. We don't
  // need a view, for instance. Using the service_worker_script as the
  // background host is just totally horrible.
  extensions::ProcessManager* process_manager =
      ExtensionSystem::Get(context_)->process_manager();
  CHECK(process_manager->CreateBackgroundHost(
      extension,
      service_worker_script,
      base::Bind(&ServiceWorkerManager::ContinueRegistrationWithExtensionHost,
                 WeakThis(),
                 extension->id(),
                 extension->GetResourceURL("/*"),
                 service_worker_script)));
}

void ServiceWorkerManager::ContinueRegistrationWithExtensionHost(
    const ExtensionId& extension_id,
    const GURL& scope,
    const GURL& service_worker_script) {
  extensions::ProcessManager* process_manager =
      ExtensionSystem::Get(context_)->process_manager();
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_id);

  GetSWContext(extension_id)->RegisterServiceWorker(
      scope,
      service_worker_script,
      host->render_process_host()->GetID(),
      base::Bind(
          &ServiceWorkerManager::FinishRegistration, WeakThis(), extension_id));
}

void ServiceWorkerManager::FinishRegistration(const ExtensionId& extension_id,
                                              ServiceWorkerStatusCode result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

void ServiceWorkerManager::UnregisterExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  GetSWContext(extension->id())->UnregisterServiceWorker(
      extension->GetResourceURL("/*"),
      -1,
      base::Bind(&ServiceWorkerManager::FinishUnregistration,
                 WeakThis(),
                 extension->id()));
}

void ServiceWorkerManager::FinishUnregistration(
    const ExtensionId& extension_id,
    ServiceWorkerStatusCode result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
