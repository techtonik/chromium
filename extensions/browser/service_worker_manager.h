// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/common/service_worker_status_code.h"
#include "extensions/common/extension.h"

namespace content {
class ServiceWorkerContext;
class ServiceWorkerRegistration;
class StoragePartition;
}

namespace extensions {

// This class registers and unregisters Service Workers for extensions that use
// them and allows clients to look up the Service Worker for an extension.
//
// This class lives on the UI thread despite interacting with the
// ServiceWorkerContextCore that lives on the IO thread.
//
// See
// https://docs.google.com/document/d/1szeOHrr_qEJGSNbDtEqeKcGDkLmwvftqTV731kQw2rM/edit
// for more details.
class ServiceWorkerManager : public KeyedService {
 public:
  // Convenience function to get the ServiceWorkerManager for a BrowserContext.
  static ServiceWorkerManager* Get(content::BrowserContext* context);

  // Makes sure a ServiceWorker is registered for |extension|. This immediately
  // cancels callbacks waiting for an unregistration. If multiple registrations
  // and unregistrations are in flight concurrently, only the last one takes
  // effect.
  void RegisterExtension(const Extension* extension);
  // Unregisters any ServiceWorker for |extension|. This immediately cancels
  // callbacks waiting for a registration, and has the same response to multiple
  // in-flight calls as RegisterExtension.
  void UnregisterExtension(const Extension* extension);

 private:
  friend class ServiceWorkerManagerFactory;

  ServiceWorkerManager(content::BrowserContext* context);
  virtual ~ServiceWorkerManager();

  inline content::StoragePartition* GetStoragePartition(
      const ExtensionId& ext_id) const;
  inline content::ServiceWorkerContext* GetSWContext(
      const ExtensionId& ext_id) const;
  inline base::WeakPtr<ServiceWorkerManager> WeakThis();

  void ContinueRegistrationWithExtensionHost(const ExtensionId& extension_id,
                                             const GURL& scope,
                                             const GURL& service_worker_script);
  void ContinueUnregistrationWithExtensionHost(const ExtensionId& extension_id,
                                               const GURL& scope);
  void FinishRegistration(const ExtensionId& extension_id,
                          content::ServiceWorkerStatusCode result);
  void FinishUnregistration(const ExtensionId& extension_id,
                            content::ServiceWorkerStatusCode result);

  content::BrowserContext* const context_;

  enum RegistrationState {
    // Represented by not being in the map.
    UNREGISTERED,
    // Between a call to RegisterExtension and the response from
    REGISTERING,
    REGISTERED,
    UNREGISTERING,
  };
  struct State {
    RegistrationState registration;
    int outstanding_state_changes;
    // These two can be non-empty during REGISTERING.
    std::vector<base::Closure> registration_succeeded;
    std::vector<base::Closure> registration_failed;
    // These two can be non-empty during UNREGISTERING.
    std::vector<base::Closure> unregistration_succeeded;
    std::vector<base::Closure> unregistration_failed;
    State();
    ~State();
  };
  base::hash_map<ExtensionId, State> states_;

  base::WeakPtrFactory<ServiceWorkerManager> weak_this_factory_;
};

class ServiceWorkerManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ServiceWorkerManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ServiceWorkerManagerFactory* GetInstance();

  void SetInstanceForTesting(content::BrowserContext* context,
                             ServiceWorkerManager* prefs);

 private:
  friend struct DefaultSingletonTraits<ServiceWorkerManagerFactory>;

  ServiceWorkerManagerFactory();
  virtual ~ServiceWorkerManagerFactory();

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

}  // namespace extensions
