// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "base/process/memory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

void Noop() {}

}  // namespace

GpuMemoryBufferImplSharedMemory::GpuMemoryBufferImplSharedMemory(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    scoped_ptr<base::SharedMemory> shared_memory)
    : GpuMemoryBufferImpl(id, size, format, callback),
      shared_memory_(shared_memory.Pass()) {
  DCHECK(IsFormatSupported(format));
  DCHECK(IsSizeValidForFormat(size, format));
}

GpuMemoryBufferImplSharedMemory::~GpuMemoryBufferImplSharedMemory() {
}

// static
scoped_ptr<GpuMemoryBufferImplSharedMemory>
GpuMemoryBufferImplSharedMemory::Create(gfx::GpuMemoryBufferId id,
                                        const gfx::Size& size,
                                        gfx::BufferFormat format,
                                        const DestructionCallback& callback) {
  size_t buffer_size = 0u;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &buffer_size))
    return nullptr;

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(buffer_size))
    return nullptr;

  return make_scoped_ptr(new GpuMemoryBufferImplSharedMemory(
      id, size, format, callback, shared_memory.Pass()));
}

// static
gfx::GpuMemoryBufferHandle
GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    base::ProcessHandle child_process) {
  size_t buffer_size = 0u;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &buffer_size))
    return gfx::GpuMemoryBufferHandle();

  base::SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(buffer_size))
    return gfx::GpuMemoryBufferHandle();

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id;
  shared_memory.GiveToProcess(child_process, &handle.handle);
  return handle;
}

// static
scoped_ptr<GpuMemoryBufferImplSharedMemory>
GpuMemoryBufferImplSharedMemory::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return nullptr;

  size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.handle, false));
  if (!shared_memory->Map(buffer_size))
    base::TerminateBecauseOutOfMemory(buffer_size);

  return make_scoped_ptr(new GpuMemoryBufferImplSharedMemory(
      handle.id, size, format, callback, shared_memory.Pass()));
}

// static
bool GpuMemoryBufferImplSharedMemory::IsFormatSupported(
    gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::YUV_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      return true;
    case gfx::BufferFormat::BGRX_8888:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsUsageSupported(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::MAP:
    case gfx::BufferUsage::PERSISTENT_MAP:
      return true;
    case gfx::BufferUsage::SCANOUT:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return IsFormatSupported(format) && IsUsageSupported(usage);
}

// static
bool GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
    const gfx::Size& size,
    gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
      // Compressed images must have a width and height that's evenly divisible
      // by the block size.
      return size.width() % 4 == 0 && size.height() % 4 == 0;
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return true;
    case gfx::BufferFormat::YUV_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
      for (size_t i = 0; i < num_planes; ++i) {
        size_t factor = gfx::SubsamplingFactorForBufferFormat(format, i);
        if (size.width() % factor || size.height() % factor)
          return false;
      }
      return true;
    }
    case gfx::BufferFormat::UYVY_422:
      return size.width() % 2 == 0;
  }

  NOTREACHED();
  return false;
}

// static
base::Closure GpuMemoryBufferImplSharedMemory::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  base::SharedMemory shared_memory;
  bool rv = shared_memory.CreateAnonymous(
      gfx::BufferSizeForBufferFormat(size, format));
  DCHECK(rv);
  handle->type = gfx::SHARED_MEMORY_BUFFER;
  handle->handle = base::SharedMemory::DuplicateHandle(shared_memory.handle());
  return base::Bind(&Noop);
}

bool GpuMemoryBufferImplSharedMemory::Map(void** data) {
  DCHECK(!mapped_);
  size_t offset = 0;
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format_);
  for (size_t i = 0; i < num_planes; ++i) {
    data[i] = reinterpret_cast<uint8*>(shared_memory_->memory()) + offset;
    size_t row_size = gfx::RowSizeForBufferFormat(size_.width(), format_, i);
    size_t factor = gfx::SubsamplingFactorForBufferFormat(format_, i);
    offset += row_size * (size_.height() / factor);
  }
  mapped_ = true;
  return true;
}

void GpuMemoryBufferImplSharedMemory::Unmap() {
  DCHECK(mapped_);
  mapped_ = false;
}

void GpuMemoryBufferImplSharedMemory::GetStride(int* stride) const {
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format_);
  for (size_t i = 0; i < num_planes; ++i)
    stride[i] = gfx::RowSizeForBufferFormat(size_.width(), format_, i);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplSharedMemory::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = id_;
  handle.handle = shared_memory_->handle();
  return handle;
}

}  // namespace content
