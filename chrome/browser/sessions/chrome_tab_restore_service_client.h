// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SESSIONS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_

#include "base/basictypes.h"
#include "components/sessions/core/tab_restore_service_client.h"

class Profile;

// ChromeTabRestoreServiceClient provides an implementation of
// TabRestoreServiceClient that depends on chrome/.
class ChromeTabRestoreServiceClient : public sessions::TabRestoreServiceClient {
 public:
  explicit ChromeTabRestoreServiceClient(Profile* profile);
  ~ChromeTabRestoreServiceClient() override;

 private:
  // TabRestoreServiceClient:
  TabRestoreServiceDelegate* CreateTabRestoreServiceDelegate(
      int host_desktop_type,
      const std::string& app_name) override;
  TabRestoreServiceDelegate* FindTabRestoreServiceDelegateForWebContents(
      const content::WebContents* contents) override;
  TabRestoreServiceDelegate* FindTabRestoreServiceDelegateWithID(
      SessionID::id_type desired_id,
      int host_desktop_type) override;
  bool ShouldTrackURLForRestore(const GURL& url) override;
  base::SequencedWorkerPool* GetBlockingPool() override;
  base::FilePath GetPathToSaveTo() override;
  GURL GetNewTabURL() override;
  bool HasLastSession() override;
  void GetLastSession(const sessions::GetLastSessionCallback& callback,
                      base::CancelableTaskTracker* tracker) override;
  void OnTabRestored(const GURL& url) override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTabRestoreServiceClient);
};

#endif  // CHROME_BROWSER_SESSIONS_CHROME_TAB_RESTORE_SERVICE_CLIENT_H_
