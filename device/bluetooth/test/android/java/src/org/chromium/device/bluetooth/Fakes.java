// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * Fake implementations of android.bluetooth.* classes for testing.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class Fakes {
    private static final String TAG = "cr.Bluetooth";

    /**
     * Fakes android.bluetooth.BluetoothAdapter.
     */
    static class FakeBluetoothAdapter extends Wrappers.BluetoothAdapterWrapper {
        private final FakeBluetoothLeScanner mFakeScanner;
        final long mNativeBluetoothTestAndroid;

        /**
         * Creates a FakeBluetoothAdapter.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public static FakeBluetoothAdapter create(long nativeBluetoothTestAndroid) {
            Log.v(TAG, "FakeBluetoothAdapter created.");
            return new FakeBluetoothAdapter(nativeBluetoothTestAndroid);
        }

        private FakeBluetoothAdapter(long nativeBluetoothTestAndroid) {
            super(null, new FakeBluetoothLeScanner());
            mNativeBluetoothTestAndroid = nativeBluetoothTestAndroid;
            mFakeScanner = (FakeBluetoothLeScanner) mScanner;
        }

        /**
         * Creates and discovers a new device.
         */
        @CalledByNative("FakeBluetoothAdapter")
        public void discoverLowEnergyDevice(int deviceOrdinal) {
            switch (deviceOrdinal) {
                case 1: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001800-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001801-0000-1000-8000-00805f9b34fb"));

                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(this, "01:00:00:90:1E:BE",
                                                       "FakeBluetoothDevice"),
                                                                    uuids));
                    break;
                }
                case 2: {
                    ArrayList<ParcelUuid> uuids = new ArrayList<ParcelUuid>(2);
                    uuids.add(ParcelUuid.fromString("00001802-0000-1000-8000-00805f9b34fb"));
                    uuids.add(ParcelUuid.fromString("00001803-0000-1000-8000-00805f9b34fb"));

                    mFakeScanner.mScanCallback.onScanResult(ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(new FakeBluetoothDevice(this, "01:00:00:90:1E:BE",
                                                       "FakeBluetoothDevice"),
                                                                    uuids));
                    break;
                }
                case 3: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(
                            ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "01:00:00:90:1E:BE", ""), uuids));

                    break;
                }
                case 4: {
                    ArrayList<ParcelUuid> uuids = null;
                    mFakeScanner.mScanCallback.onScanResult(
                            ScanSettings.CALLBACK_TYPE_ALL_MATCHES,
                            new FakeScanResult(
                                    new FakeBluetoothDevice(this, "02:00:00:8B:74:63", ""), uuids));

                    break;
                }
            }
        }

        // -----------------------------------------------------------------------------------------
        // BluetoothAdapterWrapper overrides:

        @Override
        public boolean isEnabled() {
            return true;
        }

        @Override
        public String getAddress() {
            return "A1:B2:C3:D4:E5:F6";
        }

        @Override
        public String getName() {
            return "FakeBluetoothAdapter";
        }

        @Override
        public int getScanMode() {
            return android.bluetooth.BluetoothAdapter.SCAN_MODE_NONE;
        }

        @Override
        public boolean isDiscovering() {
            return false;
        }
    }

    /**
     * Fakes android.bluetooth.le.BluetoothLeScanner.
     */
    static class FakeBluetoothLeScanner extends Wrappers.BluetoothLeScannerWrapper {
        public Wrappers.ScanCallbackWrapper mScanCallback;

        private FakeBluetoothLeScanner() {
            super(null);
        }

        @Override
        public void startScan(List<ScanFilter> filters, int scanSettingsScanMode,
                Wrappers.ScanCallbackWrapper callback) {
            if (mScanCallback != null) {
                throw new IllegalArgumentException(
                        "FakeBluetoothLeScanner does not support multiple scans.");
            }
            mScanCallback = callback;
        }

        @Override
        public void stopScan(Wrappers.ScanCallbackWrapper callback) {
            if (mScanCallback != callback) {
                throw new IllegalArgumentException("No scan in progress.");
            }
            mScanCallback = null;
        }
    }

    /**
     * Fakes android.bluetooth.le.ScanResult
     */
    static class FakeScanResult extends Wrappers.ScanResultWrapper {
        private final FakeBluetoothDevice mDevice;
        private final ArrayList<ParcelUuid> mUuids;

        FakeScanResult(FakeBluetoothDevice device, ArrayList<ParcelUuid> uuids) {
            super(null);
            mDevice = device;
            mUuids = uuids;
        }

        @Override
        public Wrappers.BluetoothDeviceWrapper getDevice() {
            return mDevice;
        }

        @Override
        public List<ParcelUuid> getScanRecord_getServiceUuids() {
            return mUuids;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothDevice.
     */
    static class FakeBluetoothDevice extends Wrappers.BluetoothDeviceWrapper {
        final FakeBluetoothAdapter mAdapter;
        private String mAddress;
        private String mName;
        private Wrappers.BluetoothGattCallbackWrapper mGattCallback;

        public FakeBluetoothDevice(FakeBluetoothAdapter adapter, String address, String name) {
            super(null);
            mAdapter = adapter;
            mAddress = address;
            mName = name;
        }

        // Create a call to onConnectionStateChange on the |chrome_device| using parameters
        // |status| & |connected|.
        @CalledByNative("FakeBluetoothDevice")
        private static void connectionStateChange(
                ChromeBluetoothDevice chromeDevice, int status, boolean connected) {
            FakeBluetoothDevice fakeDevice = (FakeBluetoothDevice) chromeDevice.mDevice;
            fakeDevice.mGattCallback.onConnectionStateChange(status, connected
                            ? android.bluetooth.BluetoothProfile.STATE_CONNECTED
                            : android.bluetooth.BluetoothProfile.STATE_DISCONNECTED);
        }

        // -----------------------------------------------------------------------------------------
        // Wrappers.BluetoothDeviceWrapper overrides:

        @Override
        public Wrappers.BluetoothGattWrapper connectGatt(Context context, boolean autoConnect,
                Wrappers.BluetoothGattCallbackWrapper callback) {
            if (mGattCallback != null && mGattCallback != callback) {
                throw new IllegalArgumentException(
                        "BluetoothGattWrapper doesn't support calls to connectGatt() with "
                        + "multiple distinct callbacks.");
            }
            nativeOnBluetoothDeviceConnectGattCalled(mAdapter.mNativeBluetoothTestAndroid);
            mGattCallback = callback;
            return new FakeBluetoothGatt(this);
        }

        @Override
        public String getAddress() {
            return mAddress;
        }

        @Override
        public int getBluetoothClass_getDeviceClass() {
            return 0x1F00; // Unspecified Device Class
        }

        @Override
        public int getBondState() {
            return BluetoothDevice.BOND_NONE;
        }

        @Override
        public String getName() {
            return mName;
        }
    }

    /**
     * Fakes android.bluetooth.BluetoothDevice.
     */
    static class FakeBluetoothGatt extends Wrappers.BluetoothGattWrapper {
        final FakeBluetoothDevice mDevice;

        public FakeBluetoothGatt(FakeBluetoothDevice device) {
            super(null);
            mDevice = device;
        }

        @Override
        public void disconnect() {
            nativeOnFakeBluetoothGattDisconnect(mDevice.mAdapter.mNativeBluetoothTestAndroid);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothTestAndroid C++ methods declared for access from java:

    // Binds to BluetoothAdapterAndroid::OnBluetoothDeviceConnectGattCalled.
    private static native void nativeOnBluetoothDeviceConnectGattCalled(
            long nativeBluetoothTestAndroid);

    // Binds to BluetoothAdapterAndroid::OnFakeBluetoothGattDisconnect.
    private static native void nativeOnFakeBluetoothGattDisconnect(long nativeBluetoothTestAndroid);
}
