// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

BluetoothGattConnectionAndroid::BluetoothGattConnectionAndroid(
    scoped_refptr<device::BluetoothAdapterAndroid> adapter,
    const std::string& device_address)
    : adapter_(adapter), device_address_(device_address) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());
}

BluetoothGattConnectionAndroid::~BluetoothGattConnectionAndroid() {
  Disconnect(base::Bind(&base::DoNothing));
}

std::string BluetoothGattConnectionAndroid::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnectionAndroid::IsConnected() {
  return adapter_->GetDevice(device_address_)->IsConnected();
}

void BluetoothGattConnectionAndroid::Disconnect(const base::Closure& callback) {
  adapter_->GetDevice(device_address_)
      ->Disconnect(callback,
                   BluetoothDevice::ErrorCallback()  // Do nothing on error.
                   );
}

}  // namespace device
