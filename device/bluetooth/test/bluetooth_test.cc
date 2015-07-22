// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothTestBase::BluetoothTestBase() {
}

BluetoothTestBase::~BluetoothTestBase() {
}

void BluetoothTestBase::Callback() {
  ++callback_count_;
}

void BluetoothTestBase::DiscoverySessionCallback(
    scoped_ptr<BluetoothDiscoverySession> discovery_session) {
  ++callback_count_;
  discovery_sessions_.push_back(discovery_session.release());
}

void BluetoothTestBase::GattConnectionCallback(
    scoped_ptr<BluetoothGattConnection> connection) {
  ++callback_count_;
  gatt_connections_.push_back(connection.release());
}

void BluetoothTestBase::ErrorCallback() {
  ++error_callback_count_;
}

void BluetoothTestBase::ConnectErrorCallback(
    enum BluetoothDevice::ConnectErrorCode error_code) {
  ++error_callback_count_;
  last_connect_error_code_ = error_code;
}

base::Closure BluetoothTestBase::GetCallback() {
  return base::Bind(&BluetoothTestBase::Callback, base::Unretained(this));
}

BluetoothAdapter::DiscoverySessionCallback
BluetoothTestBase::GetDiscoverySessionCallback() {
  return base::Bind(&BluetoothTestBase::DiscoverySessionCallback,
                    base::Unretained(this));
}

BluetoothDevice::GattConnectionCallback
BluetoothTestBase::GetGattConnectionCallback() {
  return base::Bind(&BluetoothTestBase::GattConnectionCallback,
                    base::Unretained(this));
}

BluetoothAdapter::ErrorCallback BluetoothTestBase::GetErrorCallback() {
  return base::Bind(&BluetoothTestBase::ErrorCallback, base::Unretained(this));
}

BluetoothDevice::ConnectErrorCallback
BluetoothTestBase::GetConnectErrorCallback() {
  return base::Bind(&BluetoothTestBase::ConnectErrorCallback,
                    base::Unretained(this));
}

}  // namespace device
