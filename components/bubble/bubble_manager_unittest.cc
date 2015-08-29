// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"
#include "components/bubble/bubble_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockBubbleUI : public BubbleUI {
 public:
  MockBubbleUI() {}
  ~MockBubbleUI() override { Destroyed(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(UpdateAnchorPosition, void());

  // To verify destructor call.
  MOCK_METHOD0(Destroyed, void());
};

class MockBubbleDelegate : public BubbleDelegate {
 public:
  MockBubbleDelegate() {}
  ~MockBubbleDelegate() override { Destroyed(); }

  // Default bubble shows UI and closes when asked to close.
  static scoped_ptr<MockBubbleDelegate> Default();

  // Stubborn bubble shows UI and doesn't want to close.
  static scoped_ptr<MockBubbleDelegate> Stubborn();

  MOCK_METHOD1(ShouldClose, bool(BubbleCloseReason reason));

  // A scoped_ptr can't be returned in MOCK_METHOD.
  MOCK_METHOD0(BuildBubbleUIMock, BubbleUI*());
  scoped_ptr<BubbleUI> BuildBubbleUI() override {
    return make_scoped_ptr(BuildBubbleUIMock());
  }

  // To verify destructor call.
  MOCK_METHOD0(Destroyed, void());
};

// static
scoped_ptr<MockBubbleDelegate> MockBubbleDelegate::Default() {
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(new MockBubbleUI));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::Return(true));
  return make_scoped_ptr(delegate);
}

// static
scoped_ptr<MockBubbleDelegate> MockBubbleDelegate::Stubborn() {
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(new MockBubbleUI));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillRepeatedly(testing::Return(false));
  return make_scoped_ptr(delegate);
}

// Helper class used to test chaining another bubble.
class DelegateChainHelper {
 public:
  DelegateChainHelper(BubbleManager* manager,
                      scoped_ptr<BubbleDelegate> next_delegate);

  // Will show the bubble in |next_delegate_|.
  void Chain() { manager_->ShowBubble(next_delegate_.Pass()); }

  // True if the bubble was taken by the bubble manager.
  bool BubbleWasTaken() { return !next_delegate_; }

 private:
  BubbleManager* manager_;  // Weak.
  scoped_ptr<BubbleDelegate> next_delegate_;
};

DelegateChainHelper::DelegateChainHelper(
    BubbleManager* manager,
    scoped_ptr<BubbleDelegate> next_delegate)
    : manager_(manager), next_delegate_(next_delegate.Pass()) {}

class BubbleManagerTest : public testing::Test {
 public:
  BubbleManagerTest();
  ~BubbleManagerTest() override {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<BubbleManager> manager_;
};

BubbleManagerTest::BubbleManagerTest() {}

void BubbleManagerTest::SetUp() {
  testing::Test::SetUp();
  manager_.reset(new BubbleManager);
}

void BubbleManagerTest::TearDown() {
  manager_.reset();
  testing::Test::TearDown();
}

TEST_F(BubbleManagerTest, ManagerShowsBubbleUI) {
  // Manager will delete bubble_ui.
  MockBubbleUI* bubble_ui = new MockBubbleUI;
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show());
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition()).Times(0);

  // Manager will delete delegate.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, Destroyed());
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(bubble_ui));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::Return(true));

  manager_->ShowBubble(make_scoped_ptr(delegate));
}

TEST_F(BubbleManagerTest, ManagerUpdatesBubbleUI) {
  // Manager will delete bubble_ui.
  MockBubbleUI* bubble_ui = new MockBubbleUI;
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show());
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition());

  // Manager will delete delegate.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, Destroyed());
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(bubble_ui));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::Return(true));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseOnReferenceInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseOnStubbornReferenceDoesNotInvalidate) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesMixAppropriately) {
  BubbleReference stubborn_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(stubborn_ref1);
  ASSERT_TRUE(stubborn_ref2);
  ASSERT_TRUE(stubborn_ref3);
  ASSERT_FALSE(normal_ref1);
  ASSERT_FALSE(normal_ref2);
  ASSERT_FALSE(normal_ref3);
}

TEST_F(BubbleManagerTest, UpdateAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->UpdateAllBubbleAnchors();

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get an update event because it's already closed.
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get a close event because it's already closed.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);
}

TEST_F(BubbleManagerTest, AllowBubbleChainingOnClose) {
  scoped_ptr<BubbleDelegate> chained_delegate = MockBubbleDelegate::Default();
  DelegateChainHelper chain_helper(manager_.get(), chained_delegate.Pass());

  // Manager will delete delegate.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(new MockBubbleUI));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  BubbleReference ref = manager_->ShowBubble(make_scoped_ptr(delegate));
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

TEST_F(BubbleManagerTest, AllowBubbleChainingOnCloseAll) {
  scoped_ptr<BubbleDelegate> chained_delegate = MockBubbleDelegate::Default();
  DelegateChainHelper chain_helper(manager_.get(), chained_delegate.Pass());

  // Manager will delete delegate.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(new MockBubbleUI));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);

  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

TEST_F(BubbleManagerTest, BubblesDoNotChainOnDestroy) {
  // Manager will delete delegate.
  MockBubbleDelegate* chained_delegate = new MockBubbleDelegate;
  EXPECT_CALL(*chained_delegate, BuildBubbleUIMock()).Times(0);
  EXPECT_CALL(*chained_delegate, ShouldClose(testing::_)).Times(0);

  DelegateChainHelper chain_helper(manager_.get(),
                                   make_scoped_ptr(chained_delegate));

  // Manager will delete delegate.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, BuildBubbleUIMock())
      .WillOnce(testing::Return(new MockBubbleUI));
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_.reset();

  // The manager will take the bubble, but not show it.
  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

}  // namespace
