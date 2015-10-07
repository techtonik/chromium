// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "ui/views/controls/scroll_view.h"

class Browser;
class BrowserActionsContainer;
class WrenchMenu;

// ExtensionToolbarMenuView is the view containing the extension actions that
// overflowed from the BrowserActionsContainer, and is contained in and owned by
// the app menu.
// In the event that the app menu was opened for an Extension Action drag-and-
// drop, this will also close the menu upon completion.
class ExtensionToolbarMenuView : public views::ScrollView,
                                 public BrowserActionsContainerObserver {
 public:
  ExtensionToolbarMenuView(Browser* browser, WrenchMenu* app_menu);
  ~ExtensionToolbarMenuView() override;

  // Returns whether the app menu should show this view. This is true when
  // either |container_| has icons to display or the menu was opened for a drag-
  // and-drop operation.
  bool ShouldShow();

  // views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  BrowserActionsContainer* container_for_testing() {
    return container_;
  }

  // Sets the time delay the app menu takes to close after a drag-and-drop
  // operation.
  static void set_close_menu_delay_for_testing(int delay);

 private:
  // BrowserActionsContainerObserver:
  void OnBrowserActionsContainerDestroyed(
      BrowserActionsContainer* browser_actions_container) override;
  void OnBrowserActionDragDone() override;

  // Closes the |app_menu_|.
  void CloseAppMenu();

  // Returns the padding before the BrowserActionsContainer in the menu.
  int start_padding() const;

  // The associated browser.
  Browser* browser_;

  // The app menu, which may need to be closed after a drag-and-drop.
  WrenchMenu* app_menu_;

  // The overflow BrowserActionsContainer which is nested in this view.
  BrowserActionsContainer* container_;

  // The maximum allowed height for the view.
  int max_height_;

  ScopedObserver<BrowserActionsContainer, BrowserActionsContainerObserver>
      browser_actions_container_observer_;

  base::WeakPtrFactory<ExtensionToolbarMenuView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionToolbarMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_
