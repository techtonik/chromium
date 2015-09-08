// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection.h"

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothGattConnection::BluetoothGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : adapter_(adapter), device_address_(device_address) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());

  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (device) {
    owns_reference_for_connection_ = true;
    device->AddGattConnection(this);
  }
}

BluetoothGattConnection::~BluetoothGattConnection() {
  Disconnect();
}

const std::string& BluetoothGattConnection::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnection::IsConnected() {
  if (!owns_reference_for_connection_)
    return false;
  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  DCHECK(device && device->IsGattConnected());
  return true;
}

void BluetoothGattConnection::Disconnect() {
  if (!owns_reference_for_connection_)
    return;

  owns_reference_for_connection_ = false;
  BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (device)
    device->RemoveGattConnection(this);
}

}  // namespace device
