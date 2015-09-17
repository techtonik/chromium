// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

class PrefRegistrySimple;
class PrefService;

namespace sync_driver {
class SyncService;
}

namespace password_bubble_experiment {

extern const char kBrandingExperimentName[];
extern const char kSmartLockBrandingGroupName[];

// Should be called when user dismisses the "Save Password?" dialog. It stores
// the statistics about interactions with the bubble.
void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason);

// Registers pref which contains information whether first run experience for
// the save prompt was shown.
void RegisterPrefs(PrefRegistrySimple* registry);

// A Smart Lock user is a sync user without a custom passphrase.
bool IsSmartLockUser(const sync_driver::SyncService* sync_service);

// Returns true if the password manager should be referred to as Smart Lock.
// This is only true for signed-in users.
bool IsSmartLockBrandingEnabled(const sync_driver::SyncService* sync_service);

// Returns true if save prompt should contain first run experience.
bool ShouldShowSavePromptFirstRunExperience(
    const sync_driver::SyncService* sync_service,
    PrefService* prefs);

// Sets appropriate value to the preference which controls appearance of the
// first run experience for the save prompt.
void RecordSavePromptFirstRunExperienceWasShown(PrefService* prefs);

}  // namespace password_bubble_experiment

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
