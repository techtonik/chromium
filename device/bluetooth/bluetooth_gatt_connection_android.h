// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "android/dbus/bluetooth_device_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace android {

// BluetoothGattConnectionAndroid implements BluetoothGattConnection for the
// Chrome OS platform.
class BluetoothGattConnectionAndroid
    : public device::BluetoothGattConnection,
      public BluetoothDeviceClient::Observer {
 public:
  explicit BluetoothGattConnectionAndroid(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      const dbus::ObjectPath& object_path);
  ~BluetoothGattConnectionAndroid() override;

  // BluetoothGattConnection overrides.
  std::string GetDeviceAddress() const override;
  bool IsConnected() override;
  void Disconnect(const base::Closure& callback) override;

 private:

  // android::BluetoothDeviceClient::Observer overrides.
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // True, if the connection is currently active.
  bool connected_;

  // The Bluetooth adapter that this connection is associated with.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Bluetooth address of the underlying device.
  std::string device_address_;

  // D-Bus object path of the underlying device. This is used to filter observer
  // events.
  dbus::ObjectPath object_path_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattConnectionAndroid);
};

}  // namespace android

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_ANDROID_H_
