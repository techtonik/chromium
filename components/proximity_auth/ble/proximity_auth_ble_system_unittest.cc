// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/cryptauth/mock_cryptauth_client.h"
#include "components/proximity_auth/mock_proximity_auth_client.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace proximity_auth {

namespace {

const char kTestUser[] = "example@gmail.com";

class MockConnectionFinder : public ConnectionFinder {
 public:
  MockConnectionFinder() {}
  ~MockConnectionFinder() {}

  MOCK_METHOD1(Find, void(const ConnectionCallback&));
};

class MockLockHandler : public ScreenlockBridge::LockHandler {
 public:
  MockLockHandler() {}
  ~MockLockHandler() {}

  // ScreenlockBridge::LockHandler:
  MOCK_METHOD1(ShowBannerMessage, void(const base::string16& message));
  MOCK_METHOD2(ShowUserPodCustomIcon,
               void(const std::string& user_email,
                    const ScreenlockBridge::UserPodCustomIconOptions& icon));
  MOCK_METHOD1(HideUserPodCustomIcon, void(const std::string& user_email));
  MOCK_METHOD0(EnableInput, void());
  MOCK_METHOD3(SetAuthType,
               void(const std::string& user_email,
                    AuthType auth_type,
                    const base::string16& auth_value));
  MOCK_CONST_METHOD1(GetAuthType, AuthType(const std::string& user_email));
  MOCK_CONST_METHOD0(GetScreenType, ScreenType());
  MOCK_METHOD1(Unlock, void(const std::string& user_email));
  MOCK_METHOD3(AttemptEasySignin,
               void(const std::string& user_email,
                    const std::string& secret,
                    const std::string& key_label));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLockHandler);
};

}  // namespace

class ProximityAuthBleSystemTestable : public ProximityAuthBleSystem {
 public:
  ProximityAuthBleSystemTestable(
      ScreenlockBridge* screenlock_bridge,
      ProximityAuthClient* proximity_auth_client,
      PrefService* pref_service)
      : ProximityAuthBleSystem(screenlock_bridge,
                               proximity_auth_client,
                               pref_service) {}

  ConnectionFinder* CreateConnectionFinder() override {
    return new NiceMock<MockConnectionFinder>();
  }
};

class ProximityAuthBleSystemTest : public testing::Test {
 protected:
  ProximityAuthBleSystemTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        runner_handle_(task_runner_) {}

  void SetUp() override {
    BluetoothLowEnergyDeviceWhitelist::RegisterPrefs(pref_service_.registry());

    proximity_auth_system_.reset(new ProximityAuthBleSystemTestable(
        ScreenlockBridge::Get(), &proximity_auth_client_, &pref_service_));

    ON_CALL(proximity_auth_client_, GetAuthenticatedUsername())
        .WillByDefault(Return(kTestUser));
  }

  // Injects the thread's TaskRunner for testing.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle runner_handle_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<ProximityAuthBleSystem> proximity_auth_system_;

  NiceMock<MockLockHandler> lock_handler_;
};

TEST_F(ProximityAuthBleSystemTest, LockAndUnlock_LockScreen) {
  // Lock the screen.
  ON_CALL(lock_handler_, GetScreenType())
      .WillByDefault(Return(ScreenlockBridge::LockHandler::LOCK_SCREEN));
  ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

  // Unlock the screen.
  ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

TEST_F(ProximityAuthBleSystemTest, LockAndUnlock_SigninScreen) {
  // Show the sign-in screen.
  ON_CALL(lock_handler_, GetScreenType())
      .WillByDefault(Return(ScreenlockBridge::LockHandler::SIGNIN_SCREEN));
  ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

  // Sign-in.
  ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

TEST_F(ProximityAuthBleSystemTest, LockAndUnlock_OtherScreen) {
  // Show the screen.
  ON_CALL(lock_handler_, GetScreenType())
      .WillByDefault(Return(ScreenlockBridge::LockHandler::OTHER_SCREEN));
  ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

  // Hide the screen.
  ScreenlockBridge::Get()->SetLockHandler(nullptr);
}

}  // namespace proximity_auth
