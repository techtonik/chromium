// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_command_buffer_traits.h"

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/value_state.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<gpu::CommandBuffer::State> ::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.get_offset);
  WriteParam(m, p.token);
  WriteParam(m, static_cast<int32>(p.error));
  WriteParam(m, p.generation);
}

bool ParamTraits<gpu::CommandBuffer::State> ::Read(const Message* m,
                                                   base::PickleIterator* iter,
                                                   param_type* p) {
  int32 temp;
  if (ReadParam(m, iter, &p->get_offset) &&
      ReadParam(m, iter, &p->token) &&
      ReadParam(m, iter, &temp) &&
      ReadParam(m, iter, &p->generation)) {
    p->error = static_cast<gpu::error::Error>(temp);
    return true;
  } else {
    return false;
  }
}

void ParamTraits<gpu::CommandBuffer::State> ::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<CommandBuffer::State>");
}

void ParamTraits<gpu::SyncToken>::Write(Message* m, const param_type& p) {
  const int namespace_id = static_cast<int>(p.namespace_id());
  const uint64_t command_buffer_id = p.command_buffer_id();
  const uint64_t release_count = p.release_count();

  m->WriteBytes(&namespace_id, sizeof(namespace_id));
  m->WriteBytes(&command_buffer_id, sizeof(command_buffer_id));
  m->WriteBytes(&release_count, sizeof(release_count));
}

bool ParamTraits<gpu::SyncToken>::Read(const Message* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  int namespace_id = -1;
  if (!iter->ReadInt(&namespace_id))
    return false;

  uint64_t command_buffer_id = 0;
  if (!iter->ReadUInt64(&command_buffer_id))
    return false;

  uint64_t release_count = 0;
  if (!iter->ReadUInt64(&release_count))
    return false;

  p->Set(static_cast<gpu::CommandBufferNamespace>(namespace_id),
         command_buffer_id,
         release_count);
  return true;
}

void ParamTraits<gpu::SyncToken>::Log(const param_type& p, std::string* l) {
  *l +=
      base::StringPrintf("[%d:%llX] %llu", static_cast<int>(p.namespace_id()),
                         static_cast<unsigned long long>(p.command_buffer_id()),
                         static_cast<unsigned long long>(p.release_count()));
}

void ParamTraits<gpu::Mailbox>::Write(Message* m, const param_type& p) {
  m->WriteBytes(p.name, sizeof(p.name));
}

bool ParamTraits<gpu::Mailbox> ::Read(const Message* m,
                                      base::PickleIterator* iter,
                                      param_type* p) {
  const char* bytes = NULL;
  if (!iter->ReadBytes(&bytes, sizeof(p->name)))
    return false;
  DCHECK(bytes);
  memcpy(p->name, bytes, sizeof(p->name));
  return true;
}

void ParamTraits<gpu::Mailbox>::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < sizeof(p.name); ++i)
    *l += base::StringPrintf("%02x", p.name[i]);
}

void ParamTraits<gpu::MailboxHolder>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.mailbox);
  WriteParam(m, p.texture_target);
  WriteParam(m, p.sync_point);
}

bool ParamTraits<gpu::MailboxHolder> ::Read(const Message* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  if (!ReadParam(m, iter, &p->mailbox) ||
      !ReadParam(m, iter, &p->texture_target) ||
      !ReadParam(m, iter, &p->sync_point))
    return false;
  return true;
}

void ParamTraits<gpu::MailboxHolder>::Log(const param_type& p, std::string* l) {
  ParamTraits<gpu::Mailbox>::Log(p.mailbox, l);
  *l += base::StringPrintf(":%04x@%d", p.texture_target, p.sync_point);
}

void ParamTraits<gpu::ValueState>::Write(Message* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(&p),
               sizeof(gpu::ValueState));
}

bool ParamTraits<gpu::ValueState> ::Read(const Message* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  int length;
  const char* data = NULL;
  if (!iter->ReadData(&data, &length) || length != sizeof(gpu::ValueState))
    return false;
  DCHECK(data);
  memcpy(p, data, sizeof(gpu::ValueState));
  return true;
}

void ParamTraits<gpu::ValueState>::Log(const param_type& p, std::string* l) {
  l->append("<ValueState (");
  for (int value : p.int_value)
    *l += base::StringPrintf("%i ", value);
  l->append(" int values ");
  for (float value : p.float_value)
    *l += base::StringPrintf("%f ", value);
  l->append(" float values)>");
}

}  // namespace IPC
