// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/permission_manager.h"


class Profile;

namespace content {
enum class PermissionType;
class WebContents;
};  // namespace content

class PermissionManager : public KeyedService,
                          public content::PermissionManager,
                          public content_settings::Observer {
 public:
  explicit PermissionManager(Profile* profile);
  ~PermissionManager() override;

  // content::PermissionManager implementation.
  int RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void CancelPermissionRequest(int request_id) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  content::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void RegisterPermissionUsage(content::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  struct PendingRequest;
  using PendingRequestsMap = IDMap<PendingRequest, IDMapOwnPointer>;

  struct Subscription;
  using SubscriptionsMap = IDMap<Subscription, IDMapOwnPointer>;

  void OnPermissionRequestResponse(
      int request_id,
      const base::Callback<void(content::PermissionStatus)>& callback,
      ContentSetting content_setting);

  // Not all WebContents are able to display permission requests. If the PBM
  // is required but missing for |web_contents|, don't pass along the request.
  bool IsPermissionBubbleManagerMissing(content::WebContents* web_contents);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  Profile* profile_;
  PendingRequestsMap pending_requests_;
  SubscriptionsMap subscriptions_;

  base::WeakPtrFactory<PermissionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionManager);
};

#endif // CHROME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_H_
