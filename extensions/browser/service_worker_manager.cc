// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_host.h"
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

ServiceWorkerManager::ServiceWorkerManager(BrowserContext* context)
    : context_(context), weak_this_factory_(this) {
}
ServiceWorkerManager::~ServiceWorkerManager() {
}

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
  Registration* registration = registrations_[extension->id()].get();
  //
  //
  // TODO Handle a registration object already existing?
  //
  //
  DCHECK(!registration);
  registrations_[extension->id()].reset(registration = new Registration());

  if (registration->state == REGISTERING || registration->state == REGISTERED)
    return;
  registration->state = REGISTERING;
  ++registration->outstanding_state_changes;
  const GURL service_worker_script = extension->GetResourceURL(
      BackgroundInfo::GetServiceWorkerScript(extension));

  GetSWContext(extension->id())->RegisterServiceWorker(
      extension->GetResourceURL("/*"),
      service_worker_script,
      registration,
      base::Bind(&ServiceWorkerManager::FinishRegistration,
                 WeakThis(),
                 extension->id()));
}

void ServiceWorkerManager::FinishRegistration(const ExtensionId& extension_id,
                                              bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Registration* registration = registrations_[extension_id].get();
  --registration->outstanding_state_changes;
  DCHECK_GE(registration->outstanding_state_changes, 0);
  if (registration->outstanding_state_changes > 0)
    return;

  DCHECK_EQ(registration->state, REGISTERING);
  if (success) {
    registration->state = REGISTERED;
    registration->registration_callbacks.RunSuccessCallbacksAndClear();
  } else {
    LOG(ERROR) << "Service Worker Registration failed for extension "
               << extension_id;
    registration->registration_callbacks.RunFailureCallbacksAndClear();
    registrations_.erase(extension_id);
  }
}

void ServiceWorkerManager::UnregisterExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(BackgroundInfo::HasServiceWorker(extension));

  Registration* registration = FindRegistration(extension);
  if (!registration || registration->state == UNREGISTERING)
    return;

  registration->state = UNREGISTERING;
  ++registration->outstanding_state_changes;

  GetSWContext(extension->id())->UnregisterServiceWorker(
      extension->GetResourceURL("/*"),
      base::Bind(&ServiceWorkerManager::FinishUnregistration,
                 WeakThis(),
                 extension->id()));
}

void ServiceWorkerManager::FinishUnregistration(const ExtensionId& extension_id,
                                                bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Registration* registration = registrations_[extension_id].get();
  --registration->outstanding_state_changes;
  DCHECK_GE(registration->outstanding_state_changes, 0);
  if (registration->outstanding_state_changes > 0)
    return;

  DCHECK_EQ(registration->state, UNREGISTERING);
  if (success) {
    registration->unregistration_callbacks.RunSuccessCallbacksAndClear();
    registration->installed_callbacks.RunFailureCallbacksAndClear();
    registration->activated_callbacks.RunFailureCallbacksAndClear();
    registrations_.erase(extension_id);
  } else {
    LOG(ERROR) << "Service Worker Unregistration failed for extension "
               << extension_id;
    registration->state = REGISTERED;
    registration->unregistration_callbacks.RunFailureCallbacksAndClear();
  }
}

void ServiceWorkerManager::WhenRegistered(
    const Extension* extension,
    const tracked_objects::Location& from_here,
    const base::Closure& success,
    const base::Closure& failure) {
  Registration* registration = FindRegistration(extension->id());
  if (!registration) {
    base::MessageLoop::current()->PostTask(from_here, failure);
    return;
  }

  switch (registration->state) {
    case UNREGISTERED:
    case UNREGISTERING:
      base::MessageLoop::current()->PostTask(from_here, failure);
      break;
    case REGISTERED:
      base::MessageLoop::current()->PostTask(from_here, success);
      break;
    case REGISTERING:
      registration->registration_callbacks.push_back(
          SuccessFailureClosurePair(success, failure));
      break;
  }
}

void ServiceWorkerManager::WhenUnregistered(
    const Extension* extension,
    const tracked_objects::Location& from_here,
    const base::Closure& success,
    const base::Closure& failure) {
  Registration* registration = FindRegistration(extension->id());
  if (!registration) {
    base::MessageLoop::current()->PostTask(from_here, success);
    return;
  }

  switch (registration->state) {
    case REGISTERED:
    case REGISTERING:
      base::MessageLoop::current()->PostTask(from_here, failure);
      break;
    case UNREGISTERED:
      base::MessageLoop::current()->PostTask(from_here, success);
      break;
    case UNREGISTERING:
      registration->unregistration_callbacks.push_back(
          SuccessFailureClosurePair(success, failure));
      break;
  }
}

void ServiceWorkerManager::WhenInstalled(
    const Extension* extension,
    const tracked_objects::Location& from_here,
    const base::Closure& success,
    const base::Closure& failure) {
  Registration* registration = FindRegistration(extension->id());
  if (!registration) {
    base::MessageLoop::current()->PostTask(from_here, failure);
    return;
  }

  if (registration->service_worker_host()->HasInstalled()) {
    base::MessageLoop::current()->PostTask(from_here, success);
  } else {
    registration->installed_callbacks.push_back(
        SuccessFailureClosurePair(success, failure));
  }
}

void ServiceWorkerManager::WhenActivated(
    const Extension* extension,
    const tracked_objects::Location& from_here,
    const base::Closure& success,
    const base::Closure& failure) {
  Registration* registration = FindRegistration(extension->id());
  if (!registration) {
    base::MessageLoop::current()->PostTask(from_here, failure);
    return;
  }

  if (registration->service_worker_host()->HasActivated()) {
    base::MessageLoop::current()->PostTask(from_here, success);
  } else {
    registration->activated_callbacks.push_back(
        SuccessFailureClosurePair(success, failure));
  }
}

content::ServiceWorkerHost* ServiceWorkerManager::GetServiceWorkerHost(
    ExtensionId extension_id) {
  Registration* registration = FindRegistration(extension_id);
  if (!registration)
    return NULL;
  return registration->service_worker_host();
}

WeakPtr<ServiceWorkerManager> ServiceWorkerManager::WeakThis() {
  return weak_this_factory_.GetWeakPtr();
}

// ServiceWorkerManager::SuccessFailureClosurePair

ServiceWorkerManager::SuccessFailureClosurePair::SuccessFailureClosurePair(
    base::Closure success,
    base::Closure failure)
    : success(success), failure(failure) {
}

ServiceWorkerManager::SuccessFailureClosurePair::~SuccessFailureClosurePair() {
}

void ServiceWorkerManager::VectorOfClosurePairs::RunSuccessCallbacksAndClear() {
  std::vector<SuccessFailureClosurePair> swapped_callbacks;
  swap(swapped_callbacks);
  for (size_t i = 0; i < swapped_callbacks.size(); ++i) {
    swapped_callbacks[i].success.Run();
  }
}

void ServiceWorkerManager::VectorOfClosurePairs::RunFailureCallbacksAndClear() {
  std::vector<SuccessFailureClosurePair> swapped_callbacks;
  swap(swapped_callbacks);
  for (size_t i = 0; i < swapped_callbacks.size(); ++i) {
    swapped_callbacks[i].failure.Run();
  }
}

// ServiceWorkerManager::Registration

ServiceWorkerManager::Registration::Registration()
    : state(UNREGISTERED), outstanding_state_changes(0) {
}

ServiceWorkerManager::Registration::~Registration() {
}

void ServiceWorkerManager::Registration::OnInstalled() {
  installed_callbacks.RunSuccessCallbacksAndClear();
}

void ServiceWorkerManager::Registration::OnActivated() {
  activated_callbacks.RunSuccessCallbacksAndClear();
}

bool ServiceWorkerManager::Registration::OnMessageReceived(
    const IPC::Message& message) {
  //
  //
  // TODO: Implement this.
  //
  //
  NOTIMPLEMENTED();
  return false;
}

ServiceWorkerManager::Registration* ServiceWorkerManager::FindRegistration(
    ExtensionId id) {
  RegistrationMap::iterator it = registrations_.find(id);
  if (it == registrations_.end())
    return NULL;
  else
    return it->second.get();
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
          BrowserContextDependencyManager::GetInstance()) {
}

ServiceWorkerManagerFactory::~ServiceWorkerManagerFactory() {
}

KeyedService* ServiceWorkerManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ServiceWorkerManager(context);
}

// TODO(jyasskin): Deal with incognito mode.
content::BrowserContext* ServiceWorkerManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
