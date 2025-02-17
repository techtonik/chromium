// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  explicit AutofillDataTypeController(const base::Closure& error_callback,
                                      sync_driver::SyncClient* sync_client);

  // NonUIDataTypeController implementation.
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

  // NonFrontendDatatypeController override, needed as stop-gap until bug
  // 163431 is addressed / implemented.
  void StartAssociating(const StartCallback& start_callback) override;

 protected:
  ~AutofillDataTypeController() override;

  // NonUIDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  // Callback once WebDatabase has loaded.
  void WebDatabaseLoaded();

  sync_driver::SyncClient* const sync_client_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
