// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_

#include <map>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_delegate.h"

class ExtensionToolbarMenuView;
class BookmarkMenuDelegate;
class Browser;
class WrenchMenuObserver;

namespace ui {
class NativeTheme;
}

namespace views {
class MenuButton;
struct MenuConfig;
class MenuItemView;
class MenuRunner;
class View;
}  // namespace views

// WrenchMenu adapts the WrenchMenuModel to view's menu related classes.
class WrenchMenu : public views::MenuDelegate,
                   public bookmarks::BaseBookmarkModelObserver,
                   public content::NotificationObserver {
 public:
  enum RunFlags {
    // Indicates that the menu was opened for a drag-and-drop operation.
    FOR_DROP = 1 << 0,
  };

  WrenchMenu(Browser* browser, int run_flags);
  ~WrenchMenu() override;

  void Init(ui::MenuModel* model);

  // Shows the menu relative to the specified view.
  void RunMenu(views::MenuButton* host);

  // Closes the menu if it is open, otherwise does nothing.
  void CloseMenu();

  // Whether the menu is currently visible to the user.
  bool IsShowing();

  bool for_drop() const { return (run_flags_ & FOR_DROP) != 0; }

  void AddObserver(WrenchMenuObserver* observer);
  void RemoveObserver(WrenchMenuObserver* observer);

  // MenuDelegate overrides:
  const gfx::FontList* GetLabelFontList(int command_id) const override;
  bool GetShouldUseDisabledEmphasizedForegroundColor(
      int command_id) const override;
  base::string16 GetTooltipText(int command_id,
                                const gfx::Point& p) const override;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const ui::Event& e) override;
  bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  int GetDropOperation(views::MenuItemView* item,
                       const ui::DropTargetEvent& event,
                       DropPosition* position) override;
  int OnPerformDrop(views::MenuItemView* menu,
                    DropPosition position,
                    const ui::DropTargetEvent& event) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int command_id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;
  int GetMaxWidthForMenu(views::MenuItemView* menu) override;
  bool IsItemChecked(int command_id) const override;
  bool IsCommandEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int mouse_event_flags) override;
  bool GetAccelerator(int command_id,
                      ui::Accelerator* accelerator) const override;
  void WillShowMenu(views::MenuItemView* menu) override;
  void WillHideMenu(views::MenuItemView* menu) override;
  bool ShouldCloseOnDragComplete() override;

  // bookmarks::BaseBookmarkModelObserver overrides:
  void BookmarkModelChanged() override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  ExtensionToolbarMenuView* extension_toolbar_for_testing() {
    return extension_toolbar_;
  }

 private:
  class CutCopyPasteView;
  class RecentTabsMenuModelDelegate;
  class ZoomView;

  typedef std::pair<ui::MenuModel*,int> Entry;
  typedef std::map<int,Entry> CommandIDToEntry;

  // Populates |parent| with all the child menus in |model|. Recursively invokes
  // |PopulateMenu| for any submenu.
  void PopulateMenu(views::MenuItemView* parent,
                    ui::MenuModel* model);

  // Adds a new menu item to |parent| at |menu_index| to represent the item in
  // |model| at |model_index|:
  // - |menu_index|: position in |parent| to add the new item.
  // - |model_index|: position in |model| to retrieve information about the
  //   new menu item.
  // The returned item's MenuItemView::GetCommand() is the same as that of
  // |model|->GetCommandIdAt(|model_index|).
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   int menu_index,
                                   ui::MenuModel* model,
                                   int model_index,
                                   ui::MenuModel::ItemType menu_type);

  // Invoked from the cut/copy/paste menus. Cancels the current active menu and
  // activates the menu item in |model| at |index|.
  void CancelAndEvaluate(ui::ButtonMenuItemModel* model, int index);

  // Creates the bookmark menu if necessary. Does nothing if already created or
  // the bookmark model isn't loaded.
  void CreateBookmarkMenu();

  // Returns the index of the MenuModel/index pair representing the |command_id|
  // in |command_id_to_entry_|.
  int ModelIndexFromCommandId(int command_id) const;

  // The views menu. Owned by |menu_runner_|.
  views::MenuItemView* root_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  // Maps from the command ID in model to the model/index pair the item came
  // from.
  CommandIDToEntry command_id_to_entry_;

  // Browser the menu is being shown for.
  Browser* browser_;

  // |CancelAndEvaluate| sets |selected_menu_model_| and |selected_index_|.
  // If |selected_menu_model_| is non-null after the menu completes
  // ActivatedAt is invoked. This is done so that ActivatedAt isn't invoked
  // while the message loop is nested.
  ui::ButtonMenuItemModel* selected_menu_model_;
  int selected_index_;

  // Used for managing the bookmark menu items.
  scoped_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  // Menu corresponding to IDC_BOOKMARKS_MENU.
  views::MenuItemView* bookmark_menu_;

  // Menu corresponding to IDC_FEEDBACK.
  views::MenuItemView* feedback_menu_item_;

  // Menu corresponding to IDC_TAKE_SCREENSHOT.
  views::MenuItemView* screenshot_menu_item_;

  // The view within the IDC_EXTENSIONS_OVERFLOW_MENU item (only present with
  // the toolbar action redesign enabled).
  ExtensionToolbarMenuView* extension_toolbar_;

  // Used for managing "Recent tabs" menu items.
  scoped_ptr<RecentTabsMenuModelDelegate> recent_tabs_menu_model_delegate_;

  content::NotificationRegistrar registrar_;

  // The bit mask of RunFlags.
  const int run_flags_;

  base::ObserverList<WrenchMenuObserver> observer_list_;

  // Records the time from when menu opens to when the user selects a menu item.
  base::ElapsedTimer menu_opened_timer_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_
