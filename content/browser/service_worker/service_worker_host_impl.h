// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_

#include "content/public/browser/service_worker_host.h"

#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// Implements ServiceWorkerHost.
//
// Note on Lifetime:
// Instances of this object are created and AddRef() is called in the
// constructor. Destruction is initiated by DisconnectClientAndDeleteOnUI,
// continues on the IO thread in DisconnectAndDeleteOnIO which calls Release().
// Callbacks may be outstanding that will eventually run and drop references
// to zero.
class ServiceWorkerHostImpl
    : public ServiceWorkerHost,
      public ServiceWorkerRegistration::Listener,
      public base::RefCountedThreadSafe<ServiceWorkerHostImpl> {
 public:
  ServiceWorkerHostImpl(
      const GURL& scope,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      const scoped_refptr<ServiceWorkerRegistration>& registration,
      ServiceWorkerHostClient* client);

  // ServiceWorkerHost implementation:
  virtual const GURL& scope() OVERRIDE;
  virtual const GURL& script() OVERRIDE;
  virtual bool HasInstalled() OVERRIDE;
  virtual bool HasActivated() OVERRIDE;

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // ServiceWorkerRegistration::Listener implementation:
  virtual void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) OVERRIDE;

  // Disconnects a ServiceWorkerHostClient, releasing references to it, and
  // initiates destruction of this ServiceWorkerHostImpl object.
  void DisconnectClientAndDeleteOnUI();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerHostImpl>;
  virtual ~ServiceWorkerHostImpl();

  // Completes destruction of this object on the IO thread.
  void DisconnectAndDeleteOnIO();

  // Completes handling of OnVersionAttributesChanged on UI thread by calling
  // ServiceWorkerHostClient handlers.
  void OnVersionAttributesChangedOnUI(
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info);

  const GURL scope_;
  const GURL script_;  // TODO: implement this existing.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  struct UIThreadMembers {
    UIThreadMembers(ServiceWorkerHostClient* client);
    ~UIThreadMembers();
    ServiceWorkerHostClient* client;  // Can be NULL when disconnecting.
    bool has_installed;
    bool has_activated;
  } ui_thread_;

  struct IOThreadMembers {
    IOThreadMembers(
        const scoped_refptr<ServiceWorkerRegistration>& registration);
    ~IOThreadMembers();
    scoped_refptr<ServiceWorkerRegistration> registration;
  } io_thread_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
