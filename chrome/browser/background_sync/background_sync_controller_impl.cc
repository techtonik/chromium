// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/rappor/rappor_utils.h"

BackgroundSyncControllerImpl::BackgroundSyncControllerImpl(Profile* profile)
    : profile_(profile) {}

BackgroundSyncControllerImpl::~BackgroundSyncControllerImpl() = default;

rappor::RapporService* BackgroundSyncControllerImpl::GetRapporService() {
  return g_browser_process->rappor_service();
}

void BackgroundSyncControllerImpl::NotifyBackgroundSyncRegistered(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(origin, origin.GetOrigin());

  if (profile_->IsOffTheRecord())
    return;

  rappor::SampleDomainAndRegistryFromGURL(
      GetRapporService(), "BackgroundSync.Register.Origin", origin);
}
