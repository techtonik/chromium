// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

namespace browser_sync {

using content::BrowserThread;

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public history::HistoryDBTask {
 public:
  explicit RunTaskOnHistoryThread(const base::Closure& task,
                                  TypedUrlDataTypeController* dtc)
      : task_(new base::Closure(task)),
        dtc_(dtc) {
  }

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Set the backend, then release our reference before executing the task.
    dtc_->SetBackend(backend);
    dtc_ = NULL;

    // Invoke the task, then free it immediately so we don't keep a reference
    // around all the way until DoneRunOnMainThread() is invoked back on the
    // main thread - we want to release references as soon as possible to avoid
    // keeping them around too long during shutdown.
    task_->Run();
    task_.reset();
    return true;
  }

  void DoneRunOnMainThread() override {}

 protected:
  ~RunTaskOnHistoryThread() override {}

  scoped_ptr<base::Closure> task_;
  scoped_refptr<TypedUrlDataTypeController> dtc_;
};

}  // namespace

TypedUrlDataTypeController::TypedUrlDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client)
    : NonFrontendDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          sync_client),
      backend_(NULL) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::Bind(
          &TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::Unretained(this)));
}

syncer::ModelType TypedUrlDataTypeController::type() const {
  return syncer::TYPED_URLS;
}

syncer::ModelSafeGroup TypedUrlDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_HISTORY;
}

bool TypedUrlDataTypeController::ReadyForStart() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !sync_client()->GetPrefService()->GetBoolean(
      prefs::kSavingBrowserHistoryDisabled);
}

void TypedUrlDataTypeController::SetBackend(history::HistoryBackend* backend) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  backend_ = backend;
}

void TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (sync_client()->GetPrefService()->GetBoolean(
          prefs::kSavingBrowserHistoryDisabled)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_POLICY_ERROR,
          "History saving is now disabled by policy.",
          syncer::TYPED_URLS);
      DisableImpl(error);
    }
  }
}

bool TypedUrlDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  history::HistoryService* history = sync_client()->GetHistoryService();
  if (history) {
    history->ScheduleDBTask(
        scoped_ptr<history::HistoryDBTask>(
            new RunTaskOnHistoryThread(task, this)),
        &task_tracker_);
    return true;
  } else {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service - disabling typed url sync";
    return false;
  }
}

sync_driver::SyncApiComponentFactory::SyncComponents
TypedUrlDataTypeController::CreateSyncComponents() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(backend_);
  return sync_client()
      ->GetSyncApiComponentFactory()
      ->CreateTypedUrlSyncComponents(sync_client()->GetSyncService(), backend_,
                                     this);
}

void TypedUrlDataTypeController::DisconnectProcessor(
    sync_driver::ChangeProcessor* processor) {
  static_cast<TypedUrlChangeProcessor*>(processor)->Disconnect();
}

TypedUrlDataTypeController::~TypedUrlDataTypeController() {}

}  // namespace browser_sync
