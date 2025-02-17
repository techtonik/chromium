// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_PALETTE_H_
#define UI_GFX_COLOR_PALETTE_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

const SkColor kChromeIconGrey = SkColorSetRGB(0x5A, 0x5A, 0x5A);
const SkColor kGoogleBlue = SkColorSetRGB(0x42, 0x85, 0xF4);

// The number (700) refers to the shade of darkness. Each color in the MD
// palette ranges from 100-900.
const SkColor kGoogleRed700 = SkColorSetRGB(0xC5, 0x39, 0x29);
const SkColor kGoogleGreen700 = SkColorSetRGB(0x0B, 0x80, 0x43);
const SkColor kGoogleYellow700 = SkColorSetRGB(0xF0, 0x93, 0x00);

}  // namespace gfx

#endif  // UI_GFX_COLOR_PALETTE_H_
