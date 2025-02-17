// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/menu_test_base.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

using base::ASCIIToUTF16;

// Simple test for clicking a menu item.  This template class clicks on an
// item and checks that the returned id matches.  The index of the item
// is the template argument.
template<int INDEX>
class MenuItemViewTestBasic : public MenuTestBase {
 public:
  MenuItemViewTestBasic() {
  }

  ~MenuItemViewTestBasic() override {
  }

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItemWithLabel(1, ASCIIToUTF16("item 1"));
    menu->AppendMenuItemWithLabel(2, ASCIIToUTF16("item 2"));
    menu->AppendSeparator();
    menu->AppendMenuItemWithLabel(3, ASCIIToUTF16("item 3"));
  }

  // Click on item INDEX.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(3, submenu->GetMenuItemCount());

    // click an item and pass control to the next step
    views::MenuItemView* item = submenu->GetMenuItemAt(INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestBasic::Step2));
  }

  // Check the clicked item and complete the test.
  void Step2() {
    ASSERT_FALSE(menu()->GetSubmenu()->IsShowing());
    ASSERT_EQ(INDEX + 1, last_command());
    Done();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuItemViewTestBasic);
};

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_SelectItem2 DISABLED_SelectItem2
#define MAYBE_SelectItem1 DISABLED_SelectItem1
#define MAYBE_SelectItem0 DISABLED_SelectItem0
#else
#define MAYBE_SelectItem2 SelectItem2
#define MAYBE_SelectItem1 SelectItem1
#define MAYBE_SelectItem0 SelectItem0
#endif

// Click each item of a 3-item menu (with separator).
typedef MenuItemViewTestBasic<0> MenuItemViewTestBasic0;
typedef MenuItemViewTestBasic<1> MenuItemViewTestBasic1;
typedef MenuItemViewTestBasic<2> MenuItemViewTestBasic2;
VIEW_TEST(MenuItemViewTestBasic0, MAYBE_SelectItem0)
VIEW_TEST(MenuItemViewTestBasic1, MAYBE_SelectItem1)

VIEW_TEST(MenuItemViewTestBasic2, MAYBE_SelectItem2)

// Test class for inserting a menu item while the menu is open.
template<int INSERT_INDEX, int SELECT_INDEX>
class MenuItemViewTestInsert : public MenuTestBase {
 public:
  MenuItemViewTestInsert() : inserted_item_(NULL) {
  }

  ~MenuItemViewTestInsert() override {
  }

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItemWithLabel(1, ASCIIToUTF16("item 1"));
    menu->AppendMenuItemWithLabel(2, ASCIIToUTF16("item 2"));
  }

  // Insert item at INSERT_INDEX and click item at SELECT_INDEX.
  void DoTestWithMenuOpen() override {
    LOG(ERROR) << "\nDoTestWithMenuOpen\n";
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(2, submenu->GetMenuItemCount());

    inserted_item_ = menu()->AddMenuItemAt(INSERT_INDEX,
                                           1000,
                                           ASCIIToUTF16("inserted item"),
                                           base::string16(),
                                           base::string16(),
                                           gfx::ImageSkia(),
                                           views::MenuItemView::NORMAL,
                                           ui::NORMAL_SEPARATOR);
    ASSERT_TRUE(inserted_item_);
    menu()->ChildrenChanged();

    // click an item and pass control to the next step
    views::MenuItemView* item = submenu->GetMenuItemAt(SELECT_INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestInsert::Step2));
  }

  // Check clicked item and complete test.
  void Step2() {
    LOG(ERROR) << "\nStep2\n";
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(3, submenu->GetMenuItemCount());

    if (SELECT_INDEX == INSERT_INDEX)
      ASSERT_EQ(1000, last_command());
    else if (SELECT_INDEX < INSERT_INDEX)
      ASSERT_EQ(SELECT_INDEX + 1, last_command());
    else
      ASSERT_EQ(SELECT_INDEX, last_command());

    LOG(ERROR) << "\nDone\n";
    Done();
  }

 private:
  views::MenuItemView* inserted_item_;

  DISALLOW_COPY_AND_ASSIGN(MenuItemViewTestInsert);
};

// MenuItemViewTestInsertXY inserts an item at index X and selects the
// item at index Y (after the insertion).  The tests here cover
// inserting at the beginning, middle, and end, crossbarred with
// selecting the first and last item.
typedef MenuItemViewTestInsert<0,0> MenuItemViewTestInsert00;
typedef MenuItemViewTestInsert<0,2> MenuItemViewTestInsert02;
typedef MenuItemViewTestInsert<1,0> MenuItemViewTestInsert10;
typedef MenuItemViewTestInsert<1,2> MenuItemViewTestInsert12;
typedef MenuItemViewTestInsert<2,0> MenuItemViewTestInsert20;
typedef MenuItemViewTestInsert<2,2> MenuItemViewTestInsert22;

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_InsertItem00 DISABLED_InsertItem00
#define MAYBE_InsertItem02 DISABLED_InsertItem02
#define MAYBE_InsertItem10 DISABLED_InsertItem10
#else
#define MAYBE_InsertItem00 InsertItem00
#define MAYBE_InsertItem02 InsertItem02
#define MAYBE_InsertItem10 InsertItem10
#endif
VIEW_TEST(MenuItemViewTestInsert00, MAYBE_InsertItem00)

VIEW_TEST(MenuItemViewTestInsert02, MAYBE_InsertItem02)
VIEW_TEST(MenuItemViewTestInsert10, MAYBE_InsertItem10)

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_InsertItem12 DISABLED_InsertItem12
#define MAYBE_InsertItem20 DISABLED_InsertItem20
#else
#define MAYBE_InsertItem12 InsertItem12
#define MAYBE_InsertItem20 InsertItem20
#endif
VIEW_TEST(MenuItemViewTestInsert12, MAYBE_InsertItem12)

VIEW_TEST(MenuItemViewTestInsert20, MAYBE_InsertItem20)

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_InsertItem22 DISABLED_InsertItem22
#else
#define MAYBE_InsertItem22 InsertItem22
#endif
VIEW_TEST(MenuItemViewTestInsert22, MAYBE_InsertItem22)

// Test class for inserting a menu item while a submenu is open.
template<int INSERT_INDEX>
class MenuItemViewTestInsertWithSubmenu : public MenuTestBase {
 public:
  MenuItemViewTestInsertWithSubmenu() :
      submenu_(NULL),
      inserted_item_(NULL) {
  }

  ~MenuItemViewTestInsertWithSubmenu() override {
  }

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    submenu_ = menu->AppendSubMenu(1, ASCIIToUTF16("My Submenu"));
    submenu_->AppendMenuItemWithLabel(101, ASCIIToUTF16("submenu item 1"));
    submenu_->AppendMenuItemWithLabel(101, ASCIIToUTF16("submenu item 2"));
    menu->AppendMenuItemWithLabel(2, ASCIIToUTF16("item 2"));
  }

  // Post submenu.
  void DoTestWithMenuOpen() override {
    Click(submenu_,
          CreateEventTask(this, &MenuItemViewTestInsertWithSubmenu::Step2));
  }

  // Insert item at INSERT_INDEX.
  void Step2() {
    inserted_item_ = menu()->AddMenuItemAt(INSERT_INDEX,
                                           1000,
                                           ASCIIToUTF16("inserted item"),
                                           base::string16(),
                                           base::string16(),
                                           gfx::ImageSkia(),
                                           views::MenuItemView::NORMAL,
                                           ui::NORMAL_SEPARATOR);
    ASSERT_TRUE(inserted_item_);
    menu()->ChildrenChanged();

    Click(inserted_item_,
          CreateEventTask(this, &MenuItemViewTestInsertWithSubmenu::Step3));
  }

  void Step3() {
    Done();
  }

 private:
  views::MenuItemView* submenu_;
  views::MenuItemView* inserted_item_;

  DISALLOW_COPY_AND_ASSIGN(MenuItemViewTestInsertWithSubmenu);
};

// MenuItemViewTestInsertWithSubmenuX posts a menu and its submenu,
// then inserts an item in the top-level menu at X.
typedef MenuItemViewTestInsertWithSubmenu<0> MenuItemViewTestInsertWithSubmenu0;
typedef MenuItemViewTestInsertWithSubmenu<1> MenuItemViewTestInsertWithSubmenu1;


#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_InsertItemWithSubmenu0 DISABLED_InsertItemWithSubmenu0
#define MAYBE_InsertItemWithSubmenu1 DISABLED_InsertItemWithSubmenu1
#else
#define MAYBE_InsertItemWithSubmenu0 InsertItemWithSubmenu0
#define MAYBE_InsertItemWithSubmenu1 InsertItemWithSubmenu1
#endif
VIEW_TEST(MenuItemViewTestInsertWithSubmenu0, MAYBE_InsertItemWithSubmenu0)
VIEW_TEST(MenuItemViewTestInsertWithSubmenu1, MAYBE_InsertItemWithSubmenu1)

// Test class for removing a menu item while the menu is open.
template<int REMOVE_INDEX, int SELECT_INDEX>
class MenuItemViewTestRemove : public MenuTestBase {
 public:
  MenuItemViewTestRemove() {
  }

  ~MenuItemViewTestRemove() override {
  }

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItemWithLabel(1, ASCIIToUTF16("item 1"));
    menu->AppendMenuItemWithLabel(2, ASCIIToUTF16("item 2"));
    menu->AppendMenuItemWithLabel(3, ASCIIToUTF16("item 3"));
  }

  // Remove item at REMOVE_INDEX and click item at SELECT_INDEX.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(3, submenu->GetMenuItemCount());

    // remove
    menu()->RemoveMenuItemAt(REMOVE_INDEX);
    menu()->ChildrenChanged();

    // click
    views::MenuItemView* item = submenu->GetMenuItemAt(SELECT_INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestRemove::Step2));
  }

  // Check clicked item and complete test.
  void Step2() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(2, submenu->GetMenuItemCount());

    if (SELECT_INDEX < REMOVE_INDEX)
      ASSERT_EQ(SELECT_INDEX + 1, last_command());
    else
      ASSERT_EQ(SELECT_INDEX + 2, last_command());

    Done();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuItemViewTestRemove);
};

typedef MenuItemViewTestRemove<0,0> MenuItemViewTestRemove00;
typedef MenuItemViewTestRemove<0,1> MenuItemViewTestRemove01;
typedef MenuItemViewTestRemove<1,0> MenuItemViewTestRemove10;
typedef MenuItemViewTestRemove<1,1> MenuItemViewTestRemove11;
typedef MenuItemViewTestRemove<2,0> MenuItemViewTestRemove20;
typedef MenuItemViewTestRemove<2,1> MenuItemViewTestRemove21;
#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_RemoveItem00 DISABLED_RemoveItem00
#define MAYBE_RemoveItem01 DISABLED_RemoveItem01
#else
#define MAYBE_RemoveItem00 RemoveItem00
#define MAYBE_RemoveItem01 RemoveItem01
#endif
VIEW_TEST(MenuItemViewTestRemove00, MAYBE_RemoveItem00)
VIEW_TEST(MenuItemViewTestRemove01, MAYBE_RemoveItem01)

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_RemoveItem10 DISABLED_RemoveItem10
#else
#define MAYBE_RemoveItem10 RemoveItem10
#endif
VIEW_TEST(MenuItemViewTestRemove10, MAYBE_RemoveItem10)

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_RemoveItem11 DISABLED_RemoveItem11
#else
#define MAYBE_RemoveItem11 RemoveItem11
#endif
VIEW_TEST(MenuItemViewTestRemove11, MAYBE_RemoveItem11)

#if defined(OS_WIN)
// flaky on Windows - http://crbug.com/523255
#define MAYBE_RemoveItem20 DISABLED_RemoveItem20
#define MAYBE_RemoveItem21 DISABLED_RemoveItem21
#else
#define MAYBE_RemoveItem20 RemoveItem20
#define MAYBE_RemoveItem21 RemoveItem21
#endif
VIEW_TEST(MenuItemViewTestRemove20, MAYBE_RemoveItem20)

VIEW_TEST(MenuItemViewTestRemove21, MAYBE_RemoveItem21)

// Test class for removing a menu item while a submenu is open.
template<int REMOVE_INDEX>
class MenuItemViewTestRemoveWithSubmenu : public MenuTestBase {
 public:
  MenuItemViewTestRemoveWithSubmenu() : submenu_(NULL) {
  }

  ~MenuItemViewTestRemoveWithSubmenu() override {
  }

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItemWithLabel(1, ASCIIToUTF16("item 1"));
    submenu_ = menu->AppendSubMenu(2, ASCIIToUTF16("My Submenu"));
    submenu_->AppendMenuItemWithLabel(101, ASCIIToUTF16("submenu item 1"));
    submenu_->AppendMenuItemWithLabel(102, ASCIIToUTF16("submenu item 2"));
  }

  // Post submenu.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());

    Click(submenu_,
          CreateEventTask(this, &MenuItemViewTestRemoveWithSubmenu::Step2));
  }

  // Remove item at REMOVE_INDEX and press escape to exit the menu loop.
  void Step2() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(2, submenu->GetMenuItemCount());

    // remove
    menu()->RemoveMenuItemAt(REMOVE_INDEX);
    menu()->ChildrenChanged();

    // click
    KeyPress(ui::VKEY_ESCAPE,
             CreateEventTask(this, &MenuItemViewTestRemoveWithSubmenu::Step3));
  }

  void Step3() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(1, submenu->GetMenuItemCount());

    Done();
  }

 private:
  views::MenuItemView* submenu_;

  DISALLOW_COPY_AND_ASSIGN(MenuItemViewTestRemoveWithSubmenu);
};

typedef MenuItemViewTestRemoveWithSubmenu<0> MenuItemViewTestRemoveWithSubmenu0;
typedef MenuItemViewTestRemoveWithSubmenu<1> MenuItemViewTestRemoveWithSubmenu1;

#if defined(USE_OZONE) || defined(OS_WIN)
// ozone bringup - http://crbug.com/401304
// flaky on Windows - http://crbug.com/523255
#define MAYBE_RemoveItemWithSubmenu0 DISABLED_RemoveItemWithSubmenu0
#define MAYBE_RemoveItemWithSubmenu1 DISABLED_RemoveItemWithSubmenu1
#else
#define MAYBE_RemoveItemWithSubmenu0 RemoveItemWithSubmenu0
#define MAYBE_RemoveItemWithSubmenu1 RemoveItemWithSubmenu1
#endif
VIEW_TEST(MenuItemViewTestRemoveWithSubmenu0, MAYBE_RemoveItemWithSubmenu0)
VIEW_TEST(MenuItemViewTestRemoveWithSubmenu1, MAYBE_RemoveItemWithSubmenu1)
