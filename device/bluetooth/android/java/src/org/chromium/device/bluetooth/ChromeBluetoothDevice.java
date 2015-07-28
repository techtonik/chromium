// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothDevice as necessary for C++
 * device::BluetoothDeviceAndroid.
 *
 * Lifetime is controlled by device::BluetoothDeviceAndroid.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
final class ChromeBluetoothDevice {
    private static final String TAG = "cr.Bluetooth";

    private final Wrappers.BluetoothDeviceWrapper mDevice;
    private List<ParcelUuid> mUuidsFromScan;
    Wrappers.BluetoothGattWrapper mBluetoothGatt;

    private ChromeBluetoothDevice(Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        mDevice = deviceWrapper;
        Log.v(TAG, "ChromeBluetoothDevice created.");
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothDeviceAndroid methods implemented in java:

    // Implements BluetoothDeviceAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothDeviceWrapper reference is
    // not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    private static ChromeBluetoothDevice create(Object deviceWrapper) {
        return new ChromeBluetoothDevice((Wrappers.BluetoothDeviceWrapper) deviceWrapper);
    }

    // Implements BluetoothDeviceAndroid::UpdateAdvertisedUUIDs.
    @CalledByNative
    private boolean updateAdvertisedUUIDs(List<ParcelUuid> uuidsFromScan) {
        if ((mUuidsFromScan == null && uuidsFromScan == null)
                || (mUuidsFromScan != null && mUuidsFromScan.equals(uuidsFromScan))) {
            return false;
        }
        mUuidsFromScan = uuidsFromScan;
        return true;
    }

    // Implements BluetoothDeviceAndroid::GetBluetoothClass.
    @CalledByNative
    private int getBluetoothClass() {
        return mDevice.getBluetoothClass_getDeviceClass();
    }

    // Implements BluetoothDeviceAndroid::GetAddress.
    @CalledByNative
    private String getAddress() {
        return mDevice.getAddress();
    }

    // Implements BluetoothDeviceAndroid::IsPaired.
    @CalledByNative
    private boolean isPaired() {
        return mDevice.getBondState() == BluetoothDevice.BOND_BONDED;
    }

    // Implements BluetoothDeviceAndroid::GetUUIDs.
    @CalledByNative
    private String[] getUuids() {
        int uuidCount = (mUuidsFromScan != null) ? mUuidsFromScan.size() : 0;
        String[] string_array = new String[uuidCount];
        for (int i = 0; i < uuidCount; i++) {
            string_array[i] = mUuidsFromScan.get(i).toString();
        }

        // TODO(scheib): return merged list of UUIDs from scan results and,
        // after a device is connected, discoverServices. crbug.com/508648

        return string_array;
    }

    // Implements BluetoothDeviceAndroid::CreateGattConnection.
    @CalledByNative
    private boolean createGattConnection(Context context) {
        if (mBluetoothGatt == null) {
            Log.i(TAG, "connectGatt");
            mBluetoothGatt = mDevice.connectGatt(
                    context, false /* autoConnect */, new BluetoothGattCallbackImpl());
        }
        Log.i(TAG, "BluetoothGatt.connect");
        return mBluetoothGatt.connect();
    }

    // Implements BluetoothDeviceAndroid::GetFakeBluetoothDeviceForTesting.
    @CalledByNative
    // 'Object' type must be used for return type because inner class
    // Wrappers.BluetoothDeviceWrapper reference is not handled by jni_generator.py JavaToJni.
    // http://crbug.com/505554
    private Object getBluetoothDeviceWrapperForTesting() {
        return mDevice;
    }

    // Implements BluetoothDeviceAndroid::GetDeviceName.
    @CalledByNative
    private String getDeviceName() {
        return mDevice.getName();
    }

    // Implements callbacks related to a GATT connection.
    private class BluetoothGattCallbackImpl extends Wrappers.BluetoothGattCallbackWrapper {
        @Override
        public void onConnectionStateChange(int status, int newState) {
            nativeOnConnectionStateChange(status == android.bluetooth.BluetoothGatt.GATT_SUCCESS, newState == android.bluetooth.BluetoothProfile.STATE_CONNECTED);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothDeviceAndroid::OnConnectionStateChange.
    void nativeOnConnectionStateChange(boolean success, boolean connected);
}
