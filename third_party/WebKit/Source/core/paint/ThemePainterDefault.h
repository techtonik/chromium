/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ThemePainterDefault_h
#define ThemePainterDefault_h

#include "core/paint/ThemePainter.h"

namespace blink {

class ThemePainterDefault final : public ThemePainter {
private:
    bool paintCheckbox(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintRadio(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintTextField(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintMenuList(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintMenuListButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSliderTrack(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSliderThumb(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintInnerSpinButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintProgressBar(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintTextArea(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSearchField(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSearchFieldCancelButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSearchFieldResultsDecoration(const LayoutObject&, const PaintInfo&, const IntRect&) override;
};

} // namespace blink

#endif // ThemePainerDefault_h
