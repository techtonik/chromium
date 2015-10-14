// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

// MOVE
#include "device/bluetooth/bluetooth_gatt_service.h"

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

  // TODO Clean this up some
  SimulateGattServicesDiscovered(device);
  bool foundUUID = false;
  BluetoothUUID searchUUID("00001800-0000-1000-8000-00805f9b34fb");
  for (const auto& service : device->GetGattServices())
    if (service->GetUUID() == searchUUID)
      foundUUID = true;
  EXPECT_TRUE(foundUUID);
}
#endif  // defined(OS_ANDROID)

}  // namespace device
