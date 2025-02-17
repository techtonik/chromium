// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/usb/web_usb_client_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/move.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/scoped_web_callbacks.h"
#include "content/public/common/service_registry.h"
#include "content/renderer/usb/type_converters.h"
#include "content/renderer/usb/web_usb_device_impl.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceFilter.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceInfo.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBDeviceRequestOptions.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBError.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace content {

namespace {

const char kNoServiceError[] = "USB service unavailable.";

// Generic default rejection handler for any WebUSB callbacks type. Assumes
// |CallbacksType| is a blink::WebCallbacks<T, const blink::WebUSBError&>
// for any type |T|.
template <typename CallbacksType>
void RejectCallbacksWithError(const blink::WebUSBError& error,
                              scoped_ptr<CallbacksType> callbacks) {
  callbacks->onError(error);
}

// Create a new ScopedWebCallbacks for WebUSB client callbacks, defaulting to
// a "no service" rejection.
template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> MakeScopedUSBCallbacks(
    CallbacksType* callbacks) {
  return make_scoped_web_callbacks(
      callbacks,
      base::Bind(&RejectCallbacksWithError<CallbacksType>,
                 blink::WebUSBError(blink::WebUSBError::Error::Service,
                                    base::UTF8ToUTF16(kNoServiceError))));
}

void OnGetDevicesComplete(
    ScopedWebCallbacks<blink::WebUSBClientGetDevicesCallbacks> scoped_callbacks,
    device::usb::DeviceManager* device_manager,
    mojo::Array<device::usb::DeviceInfoPtr> results) {
  blink::WebVector<blink::WebUSBDevice*>* devices =
      new blink::WebVector<blink::WebUSBDevice*>(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    device::usb::DevicePtr device;
    device_manager->GetDevice(results[i]->guid, mojo::GetProxy(&device));
    (*devices)[i] = new WebUSBDeviceImpl(
        device.Pass(), mojo::ConvertTo<blink::WebUSBDeviceInfo>(results[i]));
  }
  scoped_callbacks.PassCallbacks()->onSuccess(blink::adoptWebPtr(devices));
}

}  // namespace

WebUSBClientImpl::WebUSBClientImpl(content::ServiceRegistry* service_registry) {
  service_registry->ConnectToRemoteService(mojo::GetProxy(&device_manager_));
}

WebUSBClientImpl::~WebUSBClientImpl() {}

void WebUSBClientImpl::getDevices(
    blink::WebUSBClientGetDevicesCallbacks* callbacks) {
  auto scoped_callbacks = MakeScopedUSBCallbacks(callbacks);
  device_manager_->GetDevices(
      nullptr,
      base::Bind(&OnGetDevicesComplete, base::Passed(&scoped_callbacks),
                 base::Unretained(device_manager_.get())));
}

void WebUSBClientImpl::requestDevice(
    const blink::WebUSBDeviceRequestOptions& options,
    blink::WebUSBClientRequestDeviceCallbacks* callbacks) {
  callbacks->onError(blink::WebUSBError(blink::WebUSBError::Error::Service,
                                        base::UTF8ToUTF16("Not implemented.")));
  delete callbacks;
}

void WebUSBClientImpl::setObserver(Observer* observer) {
  if (!observer_) {
    // Set up two sequential calls to GetDeviceChanges to avoid latency.
    device_manager_->GetDeviceChanges(base::Bind(
        &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
    device_manager_->GetDeviceChanges(base::Bind(
        &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
  }

  observer_ = observer;
}

void WebUSBClientImpl::OnDeviceChangeNotification(
    device::usb::DeviceChangeNotificationPtr notification) {
  if (!observer_)
    return;

  device_manager_->GetDeviceChanges(base::Bind(
      &WebUSBClientImpl::OnDeviceChangeNotification, base::Unretained(this)));
  for (size_t i = 0; i < notification->devices_added.size(); ++i) {
    const device::usb::DeviceInfoPtr& device_info =
        notification->devices_added[i];
    device::usb::DevicePtr device;
    device_manager_->GetDevice(device_info->guid, mojo::GetProxy(&device));
    observer_->onDeviceConnected(blink::adoptWebPtr(new WebUSBDeviceImpl(
        device.Pass(), mojo::ConvertTo<blink::WebUSBDeviceInfo>(device_info))));
  }
  for (size_t i = 0; i < notification->devices_removed.size(); ++i) {
    const device::usb::DeviceInfoPtr& device_info =
        notification->devices_removed[i];
    device::usb::DevicePtr device;
    device_manager_->GetDevice(device_info->guid, mojo::GetProxy(&device));
    observer_->onDeviceDisconnected(blink::adoptWebPtr(new WebUSBDeviceImpl(
        device.Pass(), mojo::ConvertTo<blink::WebUSBDeviceInfo>(device_info))));
  }
}

}  // namespace content
