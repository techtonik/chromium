// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/linked_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/service_worker_host_client.h"
#include "extensions/common/extension.h"

namespace content {
class ServiceWorkerContext;
class ServiceWorkerHost;
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

  // Calls |success| when |extension| finishes getting registered.  If
  // |extension| is not being registered or starts being unregistered before its
  // registration completes, calls |failure| instead.
  void WhenRegistered(const Extension* extension,
                      const tracked_objects::Location& from_here,
                      const base::Closure& success,
                      const base::Closure& failure);

  // Calls |success| when |extension| finishes getting unregistered.  If
  // |extension| is not being unregistered or starts being registered again
  // before its unregistration completes, calls |failure| instead.
  void WhenUnregistered(const Extension* extension,
                        const tracked_objects::Location& from_here,
                        const base::Closure& success,
                        const base::Closure& failure);

  // Calls |success| when |extension| has an installed service worker.  Calls
  // |falure| if |extension| has not started the registration process or has
  // been unregistered.
  void WhenInstalled(const Extension* extension,
                     const tracked_objects::Location& from_here,
                     const base::Closure& success,
                     const base::Closure& failure);

  // Calls |success| when |extension| has an actived service worker.  Calls
  // |falure| if |extension| has not started the registration process or has
  // been unregistered.
  void WhenActivated(const Extension* extension,
                     const tracked_objects::Location& from_here,
                     const base::Closure& success,
                     const base::Closure& failure);

  // Returns the ServiceWorkerHost for an extension, or NULL if none registered.
  //
  //
  // TODO: Needs lifetime control, event listeners are holding onto this.
  //
  //
  content::ServiceWorkerHost* GetServiceWorkerHost(ExtensionId extension_id);

 private:
  friend class ServiceWorkerManagerFactory;

  ServiceWorkerManager(content::BrowserContext* context);
  virtual ~ServiceWorkerManager();

  inline content::StoragePartition* GetStoragePartition(
      const ExtensionId& ext_id) const;
  inline content::ServiceWorkerContext* GetSWContext(
      const ExtensionId& ext_id) const;
  inline base::WeakPtr<ServiceWorkerManager> WeakThis();

  void FinishRegistration(const ExtensionId& extension_id, bool success);
  void FinishUnregistration(const ExtensionId& extension_id, bool success);
  void ServiceWorkerHasActiveVersion(const ExtensionId& extension_id);

  content::BrowserContext* const context_;

  enum RegistrationState {
    // Represented by not being in the map.
    UNREGISTERED,
    // Between a call to RegisterExtension and the response from the
    // ServiceWorkerContext.
    REGISTERING,
    // Steady state when we can send messages to the extension.
    REGISTERED,
    // Between a call to UnregisterExtension and the response from the
    // ServiceWorkerContext.
    UNREGISTERING,
  };

  struct SuccessFailureClosurePair {
    SuccessFailureClosurePair(base::Closure success, base::Closure failure);
    ~SuccessFailureClosurePair();
    base::Closure success;
    base::Closure failure;
  };
  // Stores vector of <success, failure> pairs of callbacks.
  class VectorOfClosurePairs : public std::vector<SuccessFailureClosurePair> {
   public:
    // Runs all success / failure callbacks and then clears the vector.
    void RunSuccessCallbacksAndClear();
    void RunFailureCallbacksAndClear();
  };
  struct Registration : public content::ServiceWorkerHostClient {
    RegistrationState state;
    int outstanding_state_changes;
    // Can be non-empty during REGISTERING.
    VectorOfClosurePairs registration_callbacks;
    // Can be non-empty during UNREGISTERING.
    VectorOfClosurePairs unregistration_callbacks;
    VectorOfClosurePairs installed_callbacks;
    VectorOfClosurePairs activated_callbacks;
    Registration();
    virtual ~Registration();

    // content::ServiceWorkerHostClient interface:
    virtual void OnInstalled() OVERRIDE;
    virtual void OnActivated() OVERRIDE;

    // IPC::Listener interface:
    virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  };
  typedef base::hash_map<ExtensionId, linked_ptr<Registration> >
      RegistrationMap;

  // Returns the registration for an id from registrations_, or returns NULL.
  Registration* FindRegistration(ExtensionId id);
  Registration* FindRegistration(const Extension* extension) {
    return FindRegistration(extension->id());
  }

  RegistrationMap registrations_;

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
