// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

// BluetoothDeviceAndroid along with the Java class
// org.chromium.device.bluetooth.ChromeBluetoothDevice implement
// BluetoothDevice.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceAndroid final
    : public BluetoothDevice {
 public:
  // Create a BluetoothDeviceAndroid instance and associated Java
  // ChromeBluetoothDevice using the provided |java_bluetooth_device_wrapper|.
  //
  // The ChromeBluetoothDevice instance will hold a Java reference
  // to |bluetooth_device_wrapper|.
  //
  // TODO(scheib): Return a scoped_ptr<>, but then adapter will need to handle
  // this correctly. http://crbug.com/506416
  static BluetoothDeviceAndroid* Create(
      BluetoothAdapterAndroid* adapter,
      jobject bluetooth_device_wrapper);  // Java Type: bluetoothDeviceWrapper

  ~BluetoothDeviceAndroid() override;

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // Returns the associated ChromeBluetoothDevice Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Get owning BluetoothAdapter cast to BluetoothAdapterAndroid.
  BluetoothAdapterAndroid* GetAdapter() {
    return static_cast<BluetoothAdapterAndroid*>(adapter_);
  }

  // Updates cached copy of advertised UUIDs discovered during a scan.
  // Returns true if new UUIDs differed from cached values.
  bool UpdateAdvertisedUUIDs(
      jobject advertised_uuids);  // Java Type: List<ParcelUuid>

  // BluetoothDevice:
  uint32 GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16 GetVendorID() const override;
  uint16 GetProductID() const override;
  uint16 GetDeviceID() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDList GetUUIDs() const override;
  int16 GetInquiryRSSI() const override;
  int16 GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32 passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const ErrorCallback& error_callback) override;
  void ConnectToService(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;

  // Callback indicating when GATT client has connected/disconnected.
  // See android.bluetooth.BluetoothGattCallback.onConnectionStateChange.
  void OnConnectionStateChange(JNIEnv* env,
                               jobject jcaller,
                               int32_t status,
                               bool connected);

  // Creates Bluetooth GATT service objects and adds them to
  // BluetoothDevice::gatt_services_ if they are not already there.
  void CreateGattRemoteService(
      JNIEnv* env,
      jobject caller,
      int32_t instanceId,
      jobject bluetooth_gatt_service_wrapper);  // Java Type:
                                                // BluetoothGattServiceWrapper

 protected:
  BluetoothDeviceAndroid(BluetoothAdapterAndroid* adapter);

  // BluetoothDevice:
  std::string GetDeviceName() const override;
  void CreateGattConnectionImpl() override;
  void DisconnectGatt() override;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_device_;

  bool gatt_connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
