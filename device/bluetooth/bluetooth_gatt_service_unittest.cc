// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID)
TEST_F(BluetoothTest, GetUUID) {
  InitWithFakeAdapter();
  StartDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(),
                               GetConnectErrorCallback());
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  // Create multiple instances, verifying each can have the same UUID.
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  EXPECT_EQ(BluetoothUUID("00000000-0000-1000-8000-00805f9b34fb"),
            device->GetGattServices()[0]->GetUUID());
  EXPECT_EQ(BluetoothUUID("00000000-0000-1000-8000-00805f9b34fb"),
            device->GetGattServices()[1]->GetUUID());
}
#endif  // defined(OS_ANDROID)

}  // namespace device
