// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_proximity_auth_client.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/sys_info.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/proximity_auth/cryptauth/cryptauth_client_impl.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/version_info/version_info.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/secure_message_delegate_chromeos.h"
#endif

using proximity_auth::ScreenlockState;

ChromeProximityAuthClient::ChromeProximityAuthClient(Profile* profile)
    : profile_(profile) {
}

ChromeProximityAuthClient::~ChromeProximityAuthClient() {
}

std::string ChromeProximityAuthClient::GetAuthenticatedUsername() const {
  const SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);
  // |profile_| has to be a signed-in profile with SigninManager already
  // created. Otherwise, just crash to collect stack.
  DCHECK(signin_manager);
  return signin_manager->GetAuthenticatedUsername();
}

void ChromeProximityAuthClient::UpdateScreenlockState(ScreenlockState state) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->UpdateScreenlockState(state);
}

void ChromeProximityAuthClient::FinalizeUnlock(bool success) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->FinalizeUnlock(success);
}

void ChromeProximityAuthClient::FinalizeSignin(const std::string& secret) {
  EasyUnlockService* service = EasyUnlockService::Get(profile_);
  if (service)
    service->FinalizeSignin(secret);
}

PrefService* ChromeProximityAuthClient::GetPrefService() {
  return profile_->GetPrefs();
}

scoped_ptr<proximity_auth::SecureMessageDelegate>
ChromeProximityAuthClient::CreateSecureMessageDelegate() {
#if defined(OS_CHROMEOS)
  return make_scoped_ptr(new chromeos::SecureMessageDelegateChromeOS());
#else
  return nullptr;
#endif
}

scoped_ptr<proximity_auth::CryptAuthClientFactory>
ChromeProximityAuthClient::CreateCryptAuthClientFactory() {
  return make_scoped_ptr(new proximity_auth::CryptAuthClientFactoryImpl(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_), GetAccountId(),
      profile_->GetRequestContext(), GetDeviceClassifier()));
}

cryptauth::DeviceClassifier ChromeProximityAuthClient::GetDeviceClassifier() {
  cryptauth::DeviceClassifier device_classifier;

#if defined(OS_CHROMEOS)
  int32 major_version, minor_version, bugfix_version;
  // TODO(tengs): base::OperatingSystemVersionNumbers only works for ChromeOS.
  // We need to get different numbers for other platforms.
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  device_classifier.set_device_os_version_code(major_version);
  device_classifier.set_device_type(cryptauth::CHROME);
#endif

  const std::vector<uint32_t>& version_components =
      base::Version(version_info::GetVersionNumber()).components();
  if (version_components.size() > 0)
    device_classifier.set_device_software_version_code(version_components[0]);

  device_classifier.set_device_software_package(version_info::GetProductName());
  return device_classifier;
}

std::string ChromeProximityAuthClient::GetAccountId() {
  return SigninManagerFactory::GetForProfile(profile_)
      ->GetAuthenticatedAccountId();
}

proximity_auth::CryptAuthEnrollmentManager*
ChromeProximityAuthClient::GetCryptAuthEnrollmentManager() {
  // TODO(tengs): Return the real manager instance once it is implemented in
  // EasyUnlockService.
  return nullptr;
}

proximity_auth::CryptAuthDeviceManager*
ChromeProximityAuthClient::GetCryptAuthDeviceManager() {
  // TODO(tengs): Return the real manager instance once it is implemented in
  // EasyUnlockService.
  return nullptr;
}
