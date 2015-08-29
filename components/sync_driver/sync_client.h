// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/base/model_type.h"

class PrefService;

namespace autofill {
class AutofillWebDataService;
class AutocompleteSyncableService;
class PersonalDataManager;
}  // namespace autofill

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace syncer {
class SyncableService;
}  // namespace syncer

namespace sync_driver {

class SyncApiComponentFactory;
class SyncService;

// Interface for clients of the Sync API to plumb through necessary dependent
// components. This interface is purely for abstracting dependencies, and
// should not contain any non-trivial functional logic.
//
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
class SyncClient {
 public:
  SyncClient();

  // Returns the current SyncService instance.
  virtual SyncService* GetSyncService() = 0;

  // Returns the current profile's preference service.
  virtual PrefService* GetPrefService() = 0;

  // DataType specific service getters.
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual scoped_refptr<password_manager::PasswordStore> GetPasswordStore() = 0;
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;
  virtual scoped_refptr<autofill::AutofillWebDataService>
  GetWebDataService() = 0;

  // Returns a weak pointer to the syncable service specified by |type|.
  // Weak pointer may be unset if service is already destroyed.
  // Note: Should only be called from the model type thread.
  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) = 0;

  // Returns the current SyncApiComponentFactory instance.
  virtual SyncApiComponentFactory* GetSyncApiComponentFactory() = 0;

 protected:
  virtual ~SyncClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncClient);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
