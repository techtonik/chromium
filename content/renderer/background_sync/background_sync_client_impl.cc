// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/background_sync/background_sync_client_impl.h"

#include "content/child/background_sync/background_sync_provider_thread_proxy.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"

namespace content {

// static
void BackgroundSyncClientImpl::Create(
    int64 service_worker_registration_id,
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request) {
  new BackgroundSyncClientImpl(service_worker_registration_id, request.Pass());
}

BackgroundSyncClientImpl::~BackgroundSyncClientImpl() {}

BackgroundSyncClientImpl::BackgroundSyncClientImpl(
    int64 service_worker_registration_id,
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request)
    : service_worker_registration_id_(service_worker_registration_id),
      binding_(this, request.Pass()) {}

void BackgroundSyncClientImpl::Sync(int handle_id,
                                    const SyncCallback& callback) {
  DCHECK(!blink::Platform::current()->mainThread()->isCurrentThread());
  // Get a registration for the given handle_id from the provider. This way
  // the provider knows about the handle and can delete it once Blink releases
  // it.
  // TODO(jkarlin): Change the WebSyncPlatform to support
  // DuplicateRegistrationHandle and then this cast can go.
  BackgroundSyncProviderThreadProxy* provider =
      static_cast<BackgroundSyncProviderThreadProxy*>(
          blink::Platform::current()->backgroundSyncProvider());
  DCHECK(provider);

  // TODO(jkarlin): Find a way to claim the handle safely without requiring a
  // round-trip IPC.
  provider->DuplicateRegistrationHandle(
      handle_id, base::Bind(&BackgroundSyncClientImpl::SyncDidGetRegistration,
                            base::Unretained(this), callback));
}

void BackgroundSyncClientImpl::SyncDidGetRegistration(
    const SyncCallback& callback,
    BackgroundSyncError error,
    const SyncRegistrationPtr& registration) {
  if (error != BACKGROUND_SYNC_ERROR_NONE) {
    callback.Run(SERVICE_WORKER_EVENT_STATUS_ABORTED);
    return;
  }

  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  if (!client) {
    callback.Run(SERVICE_WORKER_EVENT_STATUS_ABORTED);
    return;
  }

  scoped_ptr<blink::WebSyncRegistration> web_registration =
      mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(registration);

  client->DispatchSyncEvent(*web_registration, callback);
}

}  // namespace content
