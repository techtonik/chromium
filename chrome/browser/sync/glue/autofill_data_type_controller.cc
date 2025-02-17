// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync_driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/util/experiments.h"

using content::BrowserThread;

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          sync_client),
      sync_client_(sync_client) {}

syncer::ModelType AutofillDataTypeController::type() const {
  return syncer::AUTOFILL;
}

syncer::ModelSafeGroup AutofillDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

void AutofillDataTypeController::WebDatabaseLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(MODEL_STARTING, state());

  OnModelLoaded();
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool AutofillDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
}

bool AutofillDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(MODEL_STARTING, state());

  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      sync_client_->GetWebDataService();

  if (!web_data_service)
    return false;

  if (web_data_service->IsDatabaseLoaded()) {
    return true;
  } else {
    web_data_service->RegisterDBLoadedCallback(base::Bind(
        &AutofillDataTypeController::WebDatabaseLoaded, this));
    return false;
  }
}

void AutofillDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state(), MODEL_LOADED);
  NonUIDataTypeController::StartAssociating(start_callback);
}

}  // namespace browser_sync
