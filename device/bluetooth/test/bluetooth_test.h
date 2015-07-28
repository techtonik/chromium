// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapter;

// A test fixture for Bluetooth that abstracts platform specifics for creating
// and controlling fake low level objects.
//
// Subclasses on each platform implement this, and are then typedef-ed to
// BluetoothTest.
class BluetoothTestBase : public testing::Test {
 public:
  BluetoothTestBase();
  ~BluetoothTestBase() override;

  // Initializes the BluetoothAdapter |adapter_| with the system adapter.
  virtual void InitWithDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with the system adapter forced
  // to be ignored as if it did not exist. This enables tests for when an
  // adapter is not present on the system.
  virtual void InitWithoutDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with a fake adapter that can be
  // controlled by this test fixture.
  virtual void InitWithFakeAdapter(){};

  // Create a fake Low Energy device and discover it.
  // |device_ordinal| selects between multiple fake device data sets to produce.
  //   1: AA:00:00:00:00:01 with simple default values.
  //   2: AA:00:00:00:00:01 with different advertised Service UUIDs vs 1.
  //   3: AA:00:00:00:00:01 with empty name, empty UUIDs.
  //   4: BB:00:00:00:00:02 with empty name, empty UUIDs.
  virtual void DiscoverLowEnergyDevice(int device_ordinal){};

  // Simulate success of implementation details of CreateGattConnection.
  virtual void CompleteGattConnection(BluetoothDevice* device){};

  // Callbacks that increment |callback_count_|, |error_callback_count_|:
  void Callback();
  void DiscoverySessionCallback(scoped_ptr<BluetoothDiscoverySession>);
  void GattConnectionCallback(scoped_ptr<BluetoothGattConnection>);
  void ErrorCallback();
  void ConnectErrorCallback(enum BluetoothDevice::ConnectErrorCode);

  // Accessors to get callbacks bound to this fixture:
  base::Closure GetCallback();
  BluetoothAdapter::DiscoverySessionCallback GetDiscoverySessionCallback();
  BluetoothDevice::GattConnectionCallback GetGattConnectionCallback();
  BluetoothAdapter::ErrorCallback GetErrorCallback();
  BluetoothDevice::ConnectErrorCallback GetConnectErrorCallback();

  // A Message loop is required by some implementations that will PostTasks and
  // by base::RunLoop().RunUntilIdle() use in this fixuture.
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;
  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;
  ScopedVector<BluetoothGattConnection> gatt_connections_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_code_ =
      BluetoothDevice::ERROR_UNKNOWN;
  int callback_count_ = 0;
  int error_callback_count_ = 0;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
