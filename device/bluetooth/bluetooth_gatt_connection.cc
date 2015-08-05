// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection.h"

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothGattConnection::BluetoothGattConnection(
    BluetoothAdapter* adapter,
    const std::string& device_address)
    : adapter_(adapter), device_address_(device_address) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());

  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (device)
    device->IncrementGattConnectionReferenceCount();
}

BluetoothGattConnection::~BluetoothGattConnection() {
  Disconnect();
}

std::string BluetoothGattConnection::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnection::IsConnected() {
  return !already_decremented_connection_reference_on_device_ &&
         adapter_->GetDevice(device_address_)->IsGattConnected();
}

void BluetoothGattConnection::Disconnect() {
  if (already_decremented_connection_reference_on_device_)
    return;

  already_decremented_connection_reference_on_device_ = true;
  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (device)
    device->DecrementGattConnectionReferenceCount();
}

}  // namespace device
