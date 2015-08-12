// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_

#include <string>

#include "base/callback.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothAdapter;

// BluetoothGattConnection represents a GATT connection to a Bluetooth device
// that has GATT services. Instances are obtained from a BluetoothDevice,
// and the connection is kept alive as long as there is at least one
// active BluetoothGattConnection object. BluetoothGattConnection objects
// automatically update themselves, when the connection is terminated by the
// operating system (e.g. due to user action).
class DEVICE_BLUETOOTH_EXPORT BluetoothGattConnection {
 public:
  BluetoothGattConnection(BluetoothAdapter* adapter,
                          const std::string& device_address);

  // Destructor automatically closes this GATT connection. If this is the last
  // remaining GATT connection and this results in a call to the OS, that call
  // may not always succeed. Users can make an explicit call to
  // BluetoothGattConnection::Close to make sure that they are notified of
  // a possible error via the callback.
  virtual ~BluetoothGattConnection();

  // Returns the Bluetooth address of the device that this connection is open
  // to.
  std::string GetDeviceAddress() const;

  // Returns true if this GATT connection is open.
  virtual bool IsConnected();

  // Disconnects this GATT connection. The device may still remain connected due
  // to other GATT connections.
  virtual void Disconnect();

 protected:
  // The Bluetooth adapter that this connection is associated with. A reference
  // is held because BluetoothGattConnection keeps the connection alive.
  scoped_refptr<BluetoothAdapter> adapter_;

  // Bluetooth address of the underlying device.
  std::string device_address_;

 private:
  bool already_decremented_connection_reference_on_device_ = false;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattConnection);
};

}  // namespace device

#endif  //  DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
