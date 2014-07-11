// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_

#include "content/public/browser/service_worker_host.h"

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code; call Send and the messages
// will be queued as needed and sent to the active service worker.
class ServiceWorkerHostImpl : public ServiceWorkerHost {
 public:
  ServiceWorkerHostImpl(
      const GURL& scope,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      ServiceWorkerHostClient* client);

  // ServiceWorkerHost implementation:
  virtual void DisconnectServiceWorkerHostClient() OVERRIDE;
  virtual const GURL& scope() OVERRIDE;
  virtual const GURL& script() OVERRIDE;
  virtual bool HasActiveVersion() OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerHostImpl>;
  virtual ~ServiceWorkerHostImpl();

  void FindRegistrationOnIO();
  void OnRegistrationFoundOnIO(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  const GURL scope_;
  const GURL script_;  // TODO: implement this existing.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  struct UIThreadMembers {
    UIThreadMembers(ServiceWorkerHostClient* client);
    ~UIThreadMembers();
    ServiceWorkerHostClient* client;  // Can be NULL when disconnecting.
  } ui_thread_;

  struct IOThreadMembers {
    IOThreadMembers();
    ~IOThreadMembers();
    scoped_refptr<ServiceWorkerRegistration> registration;
  } io_thread_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_IMPL_H_
