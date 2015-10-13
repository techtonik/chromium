// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.BluetoothGattService;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothGattService as necessary
 * for C++ device::BluetoothRemoteGattServiceAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattServiceAndroid.
 */
@JNINamespace("device")
final class BluetoothRemoteGattServiceAndroid {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattServiceAndroid;
    final Wrappers.BluetoothDeviceWrapper mDevice;
    private List<ParcelUuid> mUuidsFromScan;
    Wrappers.BluetoothGattWrapper mBluetoothGatt;
    private final BluetoothGattCallbackImpl mBluetoothGattCallbackImpl;

    private BluetoothRemoteGattServiceAndroid(
            long nativeBluetoothRemoteGattServiceAndroid, Wrappers.BluetoothDeviceWrapper deviceWrapper) {
        mNativeBluetoothRemoteGattServiceAndroid = nativeBluetoothRemoteGattServiceAndroid;
        mDevice = deviceWrapper;
        mBluetoothGattCallbackImpl = new BluetoothGattCallbackImpl();
        Log.v(TAG, "BluetoothRemoteGattServiceAndroid created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattServiceAndroidDestruction() {
        mNativeBluetoothRemoteGattServiceAndroid = 0;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattServiceAndroid methods implemented in java:

    // Implements BluetoothRemoteGattServiceAndroid::Create.
    // 'Object' type must be used because inner class Wrappers.BluetoothDeviceWrapper reference is
    // not handled by jni_generator.py JavaToJni. http://crbug.com/505554
    @CalledByNative
    private static BluetoothRemoteGattServiceAndroid create(
            long nativeBluetoothRemoteGattServiceAndroid, Object deviceWrapper) {
        return new BluetoothRemoteGattServiceAndroid(
                nativeBluetoothRemoteGattServiceAndroid, (Wrappers.BluetoothDeviceWrapper) deviceWrapper);
    }

    // Implements BluetoothRemoteGattServiceAndroid::UpdateAdvertisedUUIDs.
    @CalledByNative
    private boolean updateAdvertisedUUIDs(List<ParcelUuid> uuidsFromScan) {
        if ((mUuidsFromScan == null && uuidsFromScan == null)
                || (mUuidsFromScan != null && mUuidsFromScan.equals(uuidsFromScan))) {
            return false;
        }
        mUuidsFromScan = uuidsFromScan;
        return true;
    }

    // Implements BluetoothRemoteGattServiceAndroid::GetBluetoothClass.
    @CalledByNative
    private int getBluetoothClass() {
        return mDevice.getBluetoothClass_getDeviceClass();
    }


    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothRemoteGattServiceAndroid::OnConnectionStateChange.
    private native void nativeOnConnectionStateChange(
            long nativeBluetoothRemoteGattServiceAndroid, int status, boolean connected);

    // Binds to BluetoothRemoteGattServiceAndroid::CreateGattRemoteService.
    // 'Object' type must be used for |bluetoothGattServiceWrapper| because inner class
    // Wrappers.BluetoothGattServiceWrapper reference is not handled by jni_generator.py JavaToJni.
    // http://crbug.com/505554
    private native void nativeCreateGattRemoteService(
            long nativeBluetoothRemoteGattServiceAndroid, int instanceId, Object bluetoothGattServiceWrapper);
}
