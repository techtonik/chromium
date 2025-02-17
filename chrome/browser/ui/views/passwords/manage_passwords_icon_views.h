// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class CommandUpdater;
class ManagePasswordsUIController;

// View for the password icon in the Omnibox.
class ManagePasswordsIconViews : public ManagePasswordsIconView,
                                 public BubbleIconView {
 public:
  explicit ManagePasswordsIconViews(CommandUpdater* updater);
  ~ManagePasswordsIconViews() override;

  // ManagePasswordsIconView:
  void SetState(password_manager::ui::State state) override;
  void SetActive(bool active) override;

  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource source) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // BubbleIconView:
  views::BubbleDelegateView* GetBubble() const override;

 private:
  friend class ManagePasswordsIconViewTest;

  void UpdateVisibleUI();

  password_manager::ui::State state_;
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEWS_H_
