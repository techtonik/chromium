// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "ipc/ipc_endpoint.h"

#if defined(OS_WIN)
#include "ipc/attachment_broker_privileged_win.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "ipc/attachment_broker_privileged_mac.h"
#endif

namespace IPC {

AttachmentBrokerPrivileged::AttachmentBrokerPrivileged() {
  IPC::AttachmentBroker::SetGlobal(this);
}

AttachmentBrokerPrivileged::~AttachmentBrokerPrivileged() {
  IPC::AttachmentBroker::SetGlobal(nullptr);
}

// static
scoped_ptr<AttachmentBrokerPrivileged>
AttachmentBrokerPrivileged::CreateBroker() {
#if defined(OS_WIN)
  return scoped_ptr<AttachmentBrokerPrivileged>(
      new IPC::AttachmentBrokerPrivilegedWin);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  return scoped_ptr<AttachmentBrokerPrivileged>(
      new IPC::AttachmentBrokerPrivilegedMac);
#else
  return nullptr;
#endif
}

void AttachmentBrokerPrivileged::RegisterCommunicationChannel(
    Endpoint* endpoint) {
  endpoint->SetAttachmentBrokerEndpoint(true);
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  DCHECK(endpoints_.end() == it);
  endpoints_.push_back(endpoint);
}

void AttachmentBrokerPrivileged::DeregisterCommunicationChannel(
    Endpoint* endpoint) {
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  if (it != endpoints_.end())
    endpoints_.erase(it);
}

Sender* AttachmentBrokerPrivileged::GetSenderWithProcessId(base::ProcessId id) {
  auto it = std::find_if(endpoints_.begin(), endpoints_.end(),
                         [id](Endpoint* c) { return c->GetPeerPID() == id; });
  if (it == endpoints_.end())
    return nullptr;
  return *it;
}

void AttachmentBrokerPrivileged::LogError(UMAError error) {
  UMA_HISTOGRAM_ENUMERATION(
      "IPC.AttachmentBrokerPrivileged.BrokerAttachmentError", error, ERROR_MAX);
}

}  // namespace IPC
