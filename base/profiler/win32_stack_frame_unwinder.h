// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_
#define BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_

#include <windows.h>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {

#if !defined(_WIN64)
// Allows code to compile for x86. Actual support for x86 will require either
// refactoring these interfaces or separate architecture-specific interfaces.
using PRUNTIME_FUNCTION = void*;
#endif  // !defined(_WIN64)

// Instances of this class are expected to be created and destroyed for each
// stack unwinding. This class is not used while the target thread is suspended,
// so may allocate from the default heap.
class BASE_EXPORT Win32StackFrameUnwinder {
 public:
  // Interface for Win32 unwind-related functionality this class depends
  // on. Provides a seam for testing.
  class BASE_EXPORT UnwindFunctions {
   public:
    virtual ~UnwindFunctions();

    virtual PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                                  PDWORD64 image_base) = 0;
    virtual void VirtualUnwind(DWORD64 image_base,
                               DWORD64 program_counter,
                               PRUNTIME_FUNCTION runtime_function,
                               CONTEXT* context) = 0;
   protected:
    UnwindFunctions();

   private:
    DISALLOW_COPY_AND_ASSIGN(UnwindFunctions);
  };

  Win32StackFrameUnwinder();
  ~Win32StackFrameUnwinder();

  bool TryUnwind(CONTEXT* context);

 private:
  // This function is for internal and test purposes only.
  Win32StackFrameUnwinder(scoped_ptr<UnwindFunctions> unwind_functions);
  friend class Win32StackFrameUnwinderTest;

  // State associated with each stack unwinding.
  bool at_top_frame_;
  bool unwind_info_present_for_all_frames_;

  scoped_ptr<UnwindFunctions> unwind_functions_;

  DISALLOW_COPY_AND_ASSIGN(Win32StackFrameUnwinder);
};

}  // namespace base

#endif  // BASE_PROFILER_WIN32_STACK_FRAME_UNWINDER_H_
