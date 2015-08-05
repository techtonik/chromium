// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class FakeBluetoothDeviceClient;
class FakePowerManagerClient;

// Handler class for the Device Emulator page operations.
class DeviceEmulatorMessageHandler
    : public content::WebUIMessageHandler {
 public:
  DeviceEmulatorMessageHandler();
  ~DeviceEmulatorMessageHandler() override;

  // Adds |this| as an observer to all necessary objects.
  void Init();

  // Callback for the "requestBluetoothDiscover" message. This asynchronously
  // requests for the system to discover a certain device. The device's data
  // should be passed into |args| as a dictionary. If the device does not
  // already exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothDiscover(const base::ListValue* args);

  // Callback for the "requestBluetoothInfo" message. This asynchronously
  // requests for the devices which are already paired with the device.
  void HandleRequestBluetoothInfo(const base::ListValue* args);

  // Callback for the "requestBluetoothPair" message. This asynchronously
  // requests for the system to pair a certain device. The device's data should
  // be passed into |args| as a dictionary. If the device does not already
  // exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothPair(const base::ListValue* args);

  // Callbacks for JS update methods. All these methods work
  // asynchronously.
  void UpdateBatteryPercent(const base::ListValue* args);
  void UpdateBatteryState(const base::ListValue* args);
  void UpdateExternalPower(const base::ListValue* args);
  void UpdateTimeToEmpty(const base::ListValue* args);
  void UpdateTimeToFull(const base::ListValue* args);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Callback for the "requestPowerInfo" message. This asynchonously requests
  // for power settings such as battery percentage, external power, etc. to
  // update the view.
  void RequestPowerInfo(const base::ListValue* args);

 private:
  class BluetoothObserver;
  class PowerObserver;

  // Creates a bluetooth device with the properties given in |args|. |args|
  // should contain a dictionary so that each dictionary value can be mapped
  // to its respective property upon creating the device. Returns the device
  // path.
  std::string CreateBluetoothDeviceFromListValue(const base::ListValue* args);

  // Builds a dictionary with each key representing a property of the device
  // with path |object_path|.
  scoped_ptr<base::DictionaryValue> GetDeviceInfo(
      const dbus::ObjectPath& object_path);

  scoped_ptr<BluetoothObserver> bluetooth_observer_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  FakePowerManagerClient* fake_power_manager_client_;
  scoped_ptr<PowerObserver> power_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEmulatorMessageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
