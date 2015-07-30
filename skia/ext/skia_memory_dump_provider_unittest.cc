// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skia {

TEST(SkiaMemoryDumpProviderTest, OnMemoryDump) {
  scoped_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(nullptr));
  SkiaMemoryDumpProvider::GetInstance()->OnMemoryDump(
      process_memory_dump.get());

  ASSERT_TRUE(process_memory_dump->GetAllocatorDump("skia/sk_font_cache"));
  ASSERT_TRUE(process_memory_dump->GetAllocatorDump("skia/sk_resource_cache"));
}

}  // namespace skia
