// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace device {

class BluetoothAdapterAndroid;

// BluetoothGattConnectionAndroid implements BluetoothGattConnection for the
// Android platform.
class BluetoothGattConnectionAndroid : public device::BluetoothGattConnection {
 public:
  explicit BluetoothGattConnectionAndroid(
      scoped_refptr<device::BluetoothAdapterAndroid> adapter,
      const std::string& device_address);
  ~BluetoothGattConnectionAndroid() override;

  // BluetoothGattConnection overrides.
  std::string GetDeviceAddress() const override;
  bool IsConnected() override;
  void Disconnect(const base::Closure& callback) override;

 private:
  // True, if the connection is currently active.
  bool connected_;

  // The Bluetooth adapter that this connection is associated with.
  scoped_refptr<BluetoothAdapterAndroid> adapter_;

  // Bluetooth address of the underlying device.
  std::string device_address_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattConnectionAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_
