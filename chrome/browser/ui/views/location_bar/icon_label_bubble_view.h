// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class FontList;
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
class Painter;
}

// View used to draw a bubble, containing an icon and a label.  We use this as a
// base for the classes that handle the EV bubble, tab-to-search UI, and
// content settings.
class IconLabelBubbleView : public views::View {
 public:
  IconLabelBubbleView(int contained_image,
                      const gfx::FontList& font_list,
                      SkColor text_color,
                      SkColor parent_background_color,
                      bool elide_in_middle);
  ~IconLabelBubbleView() override;

  // Sets a background that paints |background_images| in a scalable grid.
  // Subclasses are required to call this or SetBackgroundImageWithInsets during
  // construction.
  void SetBackgroundImageGrid(const int background_images[]);

  // Divides the image designated by |background_image_id| into nine regions.
  // The four corners are specified by |insets|, the remainder are stretched to
  // fill the background. Subclasses are required to call this or
  // SetBackgroundImageGrid during construction.
  void SetBackgroundImageWithInsets(int background_image_id,
                                    gfx::Insets& insets);

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image);
  void set_is_extension_icon(bool is_extension_icon) {
    is_extension_icon_ = is_extension_icon;
  }

 protected:
  views::ImageView* image() { return image_; }
  views::Label* label() { return label_; }

  // Returns true when the background should be rendered.
  virtual bool ShouldShowBackground() const;

  // Returns a multiplier used to calculate the actual width of the view based
  // on its desired width.  This ranges from 0 for a zero-width view to 1 for a
  // full-width view and can be used to animate the width of the view.
  virtual double WidthMultiplier() const;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;

  const gfx::FontList& font_list() const { return label_->font_list(); }

  gfx::Size GetSizeForLabelWidth(int width) const;

 private:
  // Amount of padding at the edges of the bubble.  If |by_icon| is true, this
  // is the padding next to the icon; otherwise it's the padding next to the
  // label.  (We increase padding next to the label by the amount of padding
  // "built in" to the icon in order to make the bubble appear to have
  // symmetrical padding.)
  int GetBubbleOuterPadding(bool by_icon) const;

  // Sets a background color on |label_| based on |background_image_color| and
  // |parent_background_color_|.
  void SetLabelBackgroundColor(SkColor background_image_color);

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // For painting the background.
  scoped_ptr<views::Painter> background_painter_;

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;

  bool is_extension_icon_;

  SkColor parent_background_color_;

  DISALLOW_COPY_AND_ASSIGN(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
