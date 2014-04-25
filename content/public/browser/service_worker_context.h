// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "url/gurl.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

// Interface to communicate with service workers from any thread. Abstracts the
// lifetime and active version for calling code, just call Send and the messages
// will be queued as needed and sent to the active service worker.
class ServiceWorkerProxy : public IPC::Sender {
  // TODO: michaeln suggested these, but do we know we'll need them yet?
  virtual const GURL& scope() const = 0;
  virtual const GURL& script() const = 0;

  // Always returns true.
  virtual bool Send(IPC::Message* message) = 0;
};

// Represents the per-StoragePartition ServiceWorker data.  Must be used from
// the UI thread.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  typedef GURL Scope;

  typedef base::Callback<void(bool success)> ResultCallback;
  typedef base::Callback<void(bool success, const IPC::Message& message)>
      MessageCallback;
  typedef base::Callback<void(WeakPtr<ServiceWorkerProxy>)> GetWorkerCallback;

  // Equivalent to calling navigator.serviceWorker.register(script_url, {scope:
  // pattern}) from a renderer in |source_process_id|, except that |pattern| is
  // an absolute URL instead of relative to some current origin.  |callback| is
  // passed true when the JS promise is fulfilled or false when the JS
  // promise is rejected.
  //
  // The registration can fail if:
  //  * |script_url| is on a different origin from |pattern|
  //  * Fetching |script_url| fails.
  //  * |script_url| fails to parse or its top-level execution fails.
  //    TODO: The error message for this needs to be available to developers.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  virtual void RegisterServiceWorker(const Scope& pattern,
                                     const GURL& script_url,
                                     int source_process_id,
                                     const ResultCallback& callback) = 0;

  // Equivalent to calling navigator.serviceWorker.unregister(pattern) from a
  // renderer in |source_process_id|, except that |pattern| is an absolute URL
  // instead of relative to some current origin.  |callback| is passed true
  // when the JS promise is fulfilled or false when the JS promise is rejected.
  //
  // Unregistration can fail if:
  //  * No Service Worker was registered for |pattern|.
  //  * Something unexpected goes wrong, like a renderer crash.
  virtual void UnregisterServiceWorker(const Scope& pattern,
                                       int source_process_id,
                                       const ResultCallback& callback) = 0;

  // Provides a ServiceWorkerProxy object, via callback, for communicating with
  // the service worker registered for |scope|. May return NULL if there's an
  // error. Should the service worker be unregistered or for some other reason
  // become unavailable the ServiceWorkerProxy will be deleted; test the weak
  // pointer before use. 
  // 
  // Optionally provide a |listener| that will be reattached during normal
  // service worker process lifetime events of being shutdown and restarted.
  virtual void GetServiceWorkerProxy(const Scope& scope,
                                     IPC::Listener* listener,
                                     const GetWorkerCallback& callback) = 0;

  // Sends an IPC message to the active ServiceWorker whose scope is |pattern|.
  // If the worker is not running this first tries to start it. |callback| can
  // be null if the sender does not need to know if the message is successfully
  // sent or not. (If the sender expects the receiver to respond use
  // SendMessageAndRegisterCallback instead.)
  virtual void SendMessage(const Scope& pattern,
                           const IPC::Message& message,
                           const ResultCallback& callback) = 0;

  // Sends an IPC message to the active ServiceWorker whose scope is |pattern|
  // and registers |callback| to be notified when a response message is
  // received. The |callback| will be also fired with an error code if the
  // worker is unexpectedly (being) stopped. If the worker is not running this
  // first tries to start it by calling StartWorker internally.
  virtual void SendMessageAndRegisterCallback(
      const Scope& pattern,
      const IPC::Message& message,
      const MessageCallback& callback) = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
