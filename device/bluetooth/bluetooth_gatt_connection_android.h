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

// BluetoothGattConnectionAndroid implements BluetoothGattConnection for the
// Android platform.
class BluetoothGattConnectionAndroid : public device::BluetoothGattConnection {
 public:
  explicit BluetoothGattConnectionAndroid(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address);
  ~BluetoothGattConnectionAndroid() override;

  // BluetoothGattConnection overrides.
  void Disconnect(const base::Closure& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattConnectionAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_
