// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

BluetoothGattConnectionAndroid::BluetoothGattConnectionAndroid(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : BluetoothGattConnection(adapter, device_address) {}

BluetoothGattConnectionAndroid::~BluetoothGattConnectionAndroid() {
  Disconnect(base::Bind(&base::DoNothing));
}

void BluetoothGattConnectionAndroid::Disconnect(const base::Closure& callback) {
  adapter_->GetDevice(device_address_)
      ->Disconnect(callback,
                   BluetoothDevice::ErrorCallback()  // Do nothing on error.
                   );
}

}  // namespace device
