// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace IPC {
class Listener;
class Message;
}  // namespace IPC

namespace content {

class ServiceWorkerHost;

// Represents the per-StoragePartition ServiceWorker data.  Must be used from
// the UI thread.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  typedef GURL Scope;

  typedef base::Callback<void(bool success)> ResultCallback;
  typedef base::Callback<void(base::WeakPtr<ServiceWorkerHost>)>
      GetWorkerCallback;

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

  // Provides a ServiceWorkerHost object, via callback, for communicating with
  // the service worker registered for |scope|. May return NULL if there's an
  // error. Should the service worker be unregistered or for some other reason
  // become unavailable the ServiceWorkerHost will be deleted; test the weak
  // pointer before use.
  //
  // Optionally provide a |listener| that will be reattached during normal
  // service worker process lifetime events of being shutdown and restarted.
  virtual void GetServiceWorkerHost(const Scope& scope,
                                    IPC::Listener* listener,
                                    const GetWorkerCallback& callback) = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
