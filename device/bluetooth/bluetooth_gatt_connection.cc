// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection.h"

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothGattConnection::~BluetoothGattConnection() {}

std::string BluetoothGattConnection::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnection::IsConnected() {
  return adapter_->GetDevice(device_address_)->IsConnected();
}

BluetoothGattConnection::BluetoothGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : adapter_(adapter), device_address_(device_address) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());
}

void BluetoothGattConnection::IncrementConnectionReferenceCount() {
  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (!device)
    return;
  // TODO
}

void BluetoothGattConnection::DecrementConnectionReferenceCount() {
  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (!device)
    return;
  // TODO
}

}  // namespace device
