// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_delegate.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icons_public.h"

#if !defined(OS_MACOSX) && !defined(OS_IOS) && !defined(OS_ANDROID)
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

namespace infobars {

const int InfoBarDelegate::kNoIconID = 0;

InfoBarDelegate::~InfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    InfoBarDelegate::GetInfoBarAutomationType() const {
  return UNKNOWN_INFOBAR;
}

InfoBarDelegate::Type InfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

int InfoBarDelegate::GetIconId() const {
  return kNoIconID;
}

gfx::VectorIconId InfoBarDelegate::GetVectorIconId() const {
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

gfx::Image InfoBarDelegate::GetIcon() const {
#if !defined(OS_MACOSX) && !defined(OS_IOS) && !defined(OS_ANDROID)
  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::VectorIconId vector_id = GetVectorIconId();
    if (vector_id != gfx::VectorIconId::VECTOR_ICON_NONE) {
      return gfx::Image(gfx::CreateVectorIcon(
          vector_id, 18,
          GetInfoBarType() == WARNING_TYPE ? SkColorSetRGB(0xFF, 0x67, 0)
                                           : gfx::kGoogleBlue));
    }
  }
#endif

  int icon_id = GetIconId();
  return icon_id == kNoIconID ? gfx::Image() :
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(icon_id);
}

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(const NavigationDetails& details) const {
  return details.is_navigation_to_different_page &&
      !details.did_replace_entry &&
      // This next condition ensures a navigation that passes the above
      // conditions doesn't dismiss infobars added while that navigation was
      // already in process.  We carve out an exception for reloads since we
      // want reloads to dismiss infobars, but they will have unchanged entry
      // IDs.
      ((nav_entry_id_ != details.entry_id) || details.is_reload);
}

void InfoBarDelegate::InfoBarDismissed() {
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return nullptr;
}

HungRendererInfoBarDelegate* InfoBarDelegate::AsHungRendererInfoBarDelegate() {
  return nullptr;
}

InsecureContentInfoBarDelegate*
    InfoBarDelegate::AsInsecureContentInfoBarDelegate() {
  return nullptr;
}

MediaStreamInfoBarDelegate* InfoBarDelegate::AsMediaStreamInfoBarDelegate() {
  return nullptr;
}

NativeAppInfoBarDelegate* InfoBarDelegate::AsNativeAppInfoBarDelegate() {
  return nullptr;
}

PermissionInfobarDelegate* InfoBarDelegate::AsPermissionInfobarDelegate() {
  return nullptr;
}

PopupBlockedInfoBarDelegate* InfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return nullptr;
}

RegisterProtocolHandlerInfoBarDelegate*
    InfoBarDelegate::AsRegisterProtocolHandlerInfoBarDelegate() {
  return nullptr;
}

ScreenCaptureInfoBarDelegate*
    InfoBarDelegate::AsScreenCaptureInfoBarDelegate() {
  return nullptr;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return nullptr;
}

ThreeDAPIInfoBarDelegate* InfoBarDelegate::AsThreeDAPIInfoBarDelegate() {
  return nullptr;
}

translate::TranslateInfoBarDelegate*
    InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return nullptr;
}

#if defined(OS_ANDROID)
MediaThrottleInfoBarDelegate*
    InfoBarDelegate::AsMediaThrottleInfoBarDelegate() {
  return nullptr;
}
#endif

InfoBarDelegate::InfoBarDelegate() : nav_entry_id_(0) {
}

}  // namespace infobars
