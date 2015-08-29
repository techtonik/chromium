// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"
#endif

namespace content {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                                         const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         const DestructionCallback& callback)
    : id_(id),
      size_(size),
      format_(format),
      callback_(callback),
      mapped_(false),
      destruction_sync_point_(0) {}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
  DCHECK(!mapped_);
  callback_.Run(destruction_sync_point_);
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImpl::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          handle, size, format, callback);
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
#if defined(OS_ANDROID)
    case gfx::SURFACE_TEXTURE_BUFFER:
      return GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
          handle, size, format, callback);
#endif
#if defined(USE_OZONE)
    case gfx::OZONE_NATIVE_PIXMAP:
      return GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
GpuMemoryBufferImpl* GpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<GpuMemoryBufferImpl*>(buffer);
}

gfx::BufferFormat GpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

bool GpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

gfx::GpuMemoryBufferId GpuMemoryBufferImpl::GetId() const {
  return id_;
}

ClientBuffer GpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace content
