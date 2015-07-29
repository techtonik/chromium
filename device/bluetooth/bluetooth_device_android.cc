// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_gatt_connection_android.h"
#include "jni/ChromeBluetoothDevice_jni.h"

using base::android::AttachCurrentThread;
using base::android::AppendJavaStringArrayToStringVector;

namespace device {

BluetoothDeviceAndroid* BluetoothDeviceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    jobject bluetooth_device_wrapper) {  // Java Type: bluetoothDeviceWrapper
  BluetoothDeviceAndroid* device = new BluetoothDeviceAndroid(adapter);

  device->j_device_.Reset(Java_ChromeBluetoothDevice_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(device),
      bluetooth_device_wrapper));

  return device;
}

BluetoothDeviceAndroid::~BluetoothDeviceAndroid() {
}

bool BluetoothDeviceAndroid::UpdateAdvertisedUUIDs(jobject advertised_uuids) {
  return Java_ChromeBluetoothDevice_updateAdvertisedUUIDs(
      AttachCurrentThread(), j_device_.obj(), advertised_uuids);
}

// static
bool BluetoothDeviceAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);  // Generated in ChromeBluetoothDevice_jni.h
}

uint32 BluetoothDeviceAndroid::GetBluetoothClass() const {
  return Java_ChromeBluetoothDevice_getBluetoothClass(AttachCurrentThread(),
                                                      j_device_.obj());
}

std::string BluetoothDeviceAndroid::GetAddress() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothDevice_getAddress(
      AttachCurrentThread(), j_device_.obj()));
}

BluetoothDevice::VendorIDSource BluetoothDeviceAndroid::GetVendorIDSource()
    const {
  // Android API does not provide Vendor ID.
  return VENDOR_ID_UNKNOWN;
}

uint16 BluetoothDeviceAndroid::GetVendorID() const {
  // Android API does not provide Vendor ID.
  return 0;
}

uint16 BluetoothDeviceAndroid::GetProductID() const {
  // Android API does not provide Product ID.
  return 0;
}

uint16 BluetoothDeviceAndroid::GetDeviceID() const {
  // Android API does not provide Device ID.
  return 0;
}

bool BluetoothDeviceAndroid::IsPaired() const {
  return Java_ChromeBluetoothDevice_isPaired(AttachCurrentThread(),
                                             j_device_.obj());
}

bool BluetoothDeviceAndroid::IsConnected() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::IsConnectable() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::IsConnecting() const {
  NOTIMPLEMENTED();
  return false;
}

BluetoothDevice::UUIDList BluetoothDeviceAndroid::GetUUIDs() const {
  JNIEnv* env = AttachCurrentThread();
  std::vector<std::string> uuid_strings;
  AppendJavaStringArrayToStringVector(
      env, Java_ChromeBluetoothDevice_getUuids(env, j_device_.obj()).obj(),
      &uuid_strings);
  BluetoothDevice::UUIDList uuids;
  uuids.reserve(uuid_strings.size());
  for (auto uuid_string : uuid_strings) {
    uuids.push_back(BluetoothUUID(uuid_string));
  }
  return uuids;
}

int16 BluetoothDeviceAndroid::GetInquiryRSSI() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int16 BluetoothDeviceAndroid::GetInquiryTxPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothDeviceAndroid::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceAndroid::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceAndroid::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(ConnectionInfo());
}

void BluetoothDeviceAndroid::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Disconnect(const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceAndroid::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  create_gatt_connection_success_callbacks_.push_back(callback);
  create_gatt_connection_error_callbacks_.push_back(error_callback);

  //// If previous call to CreateGattConnection is already underway, wait for
  //// that response.
  // if (create_gatt_connection_error_callbacks_.size() > 1)
  //  return;

  if (!Java_ChromeBluetoothDevice_createGattConnection(
          AttachCurrentThread(), j_device_.obj(),
          base::android::GetApplicationContext())) {
    for (const auto& error_callback : create_gatt_connection_error_callbacks_)
      error_callback.Run(ERROR_FAILED);
    create_gatt_connection_success_callbacks_.clear();
    create_gatt_connection_error_callbacks_.clear();
    return;
  }
}

void BluetoothDeviceAndroid::OnConnectionStateChange(JNIEnv* env,
                                                     jobject jcaller,
                                                     bool success,
                                                     bool connected) {
  if (success && connected) {
    for (const auto& callback : create_gatt_connection_success_callbacks_)
      callback.Run(make_scoped_ptr(
          new BluetoothGattConnectionAndroid(adapter_, GetAddress())));
    create_gatt_connection_success_callbacks_.clear();
    create_gatt_connection_error_callbacks_.clear();
  } else {
    for (const auto& error_callback : create_gatt_connection_error_callbacks_)
      error_callback.Run(ERROR_FAILED);
    create_gatt_connection_success_callbacks_.clear();
    create_gatt_connection_error_callbacks_.clear();
  }
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothDeviceAndroid::GetBluetoothDeviceWrapperForTesting() {
  return Java_ChromeBluetoothDevice_getBluetoothDeviceWrapperForTesting(
      AttachCurrentThread(), j_device_.obj());
}

BluetoothDeviceAndroid::BluetoothDeviceAndroid(BluetoothAdapterAndroid* adapter)
    : adapter_(adapter) {}

std::string BluetoothDeviceAndroid::GetDeviceName() const {
  return ConvertJavaStringToUTF8(Java_ChromeBluetoothDevice_getDeviceName(
      AttachCurrentThread(), j_device_.obj()));
}

}  // namespace device
