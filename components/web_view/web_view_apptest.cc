// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/public/cpp/web_view.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/scoped_view_ptr.h"
#include "components/mus/public/cpp/tests/view_manager_test_base.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

namespace web_view {

namespace {
const char kTestOneFile[] = "test_one.html";
const char kTestOneTitle[] = "Test Title One";
const char kTestTwoFile[] = "test_two.html";
const char kTestTwoTitle[] = "Test Title Two";
const char kTestThreeFile[] = "test_three.html";
const char kTestThreeTitle[] = "Test Title Three";
const char kTheWordGreenFiveTimes[] = "the_word_green_five_times.html";
const char kTwoIframesWithGreen[] = "two_iframes_with_green.html";

GURL GetTestFileURL(const std::string& file) {
  base::FilePath data_file;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file = data_file.AppendASCII("components/test/data/web_view")
                  .AppendASCII(file)
                  .NormalizePathSeparators();
  CHECK(base::PathExists(data_file));
  return mojo::util::FilePathToFileURL(data_file);
}
}

class WebViewTest : public mus::ViewManagerTestBase,
                    public mojom::WebViewClient {
 public:
  WebViewTest()
      : web_view_(this),
        quit_condition_(NO_QUIT),
        active_find_match_(0),
        find_count_(0) {}
  ~WebViewTest() override {}

  mojom::WebView* web_view() { return web_view_.web_view(); }

  const std::string& navigation_url() const { return navigation_url_; }
  const std::string& last_title() const { return last_title_; }
  mojom::ButtonState last_back_button_state() {
    return last_back_button_state_;
  }
  mojom::ButtonState last_forward_button_state() {
    return last_forward_button_state_;
  }

  int32_t active_find_match() const { return active_find_match_; }
  int32_t find_count() const { return find_count_; }

  enum NestedLoopQuitCondition {
    NO_QUIT,
    LOADING_DONE,
    FINAL_FIND_UPATE,
  };

  void StartNestedRunLoopUntil(NestedLoopQuitCondition quit_condition) {
    quit_condition_ = quit_condition;
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  void NavigateTo(const std::string& file) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = GetTestFileURL(file).spec();
    web_view()->LoadRequest(request.Pass());
    StartNestedRunLoopUntil(LOADING_DONE);
  }

 private:
  void QuitNestedRunLoop() {
    if (run_loop_) {
      quit_condition_ = NO_QUIT;
      run_loop_->Quit();
    }
  }

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    ViewManagerTestBase::Initialize(app);
    app_ = app;
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(mus::View* root) override {
    content_ = root->connection()->CreateView();
    content_->SetBounds(root->bounds());
    root->AddChild(content_);
    content_->SetVisible(true);

    web_view_.Init(app_, content_);

    ViewManagerTestBase::OnEmbed(root);
  }

  void TearDown() override {
    mus::ScopedViewPtr::DeleteViewOrViewManager(window_manager()->GetRoot());
    ViewManagerTestBase::TearDown();
  }

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigateRequest(mojo::URLRequestPtr request) override {}
  void TopLevelNavigationStarted(const mojo::String& url) override {
    navigation_url_ = url.get();
  }
  void LoadingStateChanged(bool is_loading, double progress) override {
    if (is_loading == false && quit_condition_ == LOADING_DONE)
      QuitNestedRunLoop();
  }
  void BackForwardChanged(mojom::ButtonState back_button,
                          mojom::ButtonState forward_button) override {
    last_back_button_state_ = back_button;
    last_forward_button_state_ = forward_button;
  }
  void TitleChanged(const mojo::String& title) override {
    last_title_ = title.get();
  }
  void FindInPageMatchCountUpdated(int32_t request_id,
                                   int32_t count,
                                   bool final_update) override {
    find_count_ = count;
    if (final_update && quit_condition_ == FINAL_FIND_UPATE)
      QuitNestedRunLoop();
  }
  void FindInPageSelectionUpdated(int32_t request_id,
                                  int32_t active_match_ordinal) override {
    active_find_match_ = active_match_ordinal;
  }

  mojo::ApplicationImpl* app_;

  mus::View* content_;

  web_view::WebView web_view_;

  scoped_ptr<base::RunLoop> run_loop_;

  std::string navigation_url_;
  std::string last_title_;
  mojom::ButtonState last_back_button_state_;
  mojom::ButtonState last_forward_button_state_;

  NestedLoopQuitCondition quit_condition_;

  int32_t active_find_match_;
  int32_t find_count_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTest);
};

TEST_F(WebViewTest, TestTitleChanged) {
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestOneFile));

  // Our title should have been set on the navigation.
  EXPECT_EQ(kTestOneTitle, last_title());
}

TEST_F(WebViewTest, CanGoBackAndForward) {
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestOneFile));

  // We can't go back on first navigation since there's nothing previously on
  // the stack.
  EXPECT_EQ(GetTestFileURL(kTestOneFile).spec(), navigation_url());
  EXPECT_EQ(kTestOneTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_forward_button_state());

  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestTwoFile));

  EXPECT_EQ(kTestTwoTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_ENABLED, last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_forward_button_state());

  web_view()->GoBack();
  StartNestedRunLoopUntil(LOADING_DONE);

  EXPECT_EQ(GetTestFileURL(kTestOneFile).spec(), navigation_url());
  EXPECT_EQ(kTestOneTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_ENABLED,
            last_forward_button_state());

  web_view()->GoForward();
  StartNestedRunLoopUntil(LOADING_DONE);
  EXPECT_EQ(GetTestFileURL(kTestTwoFile).spec(), navigation_url());
  EXPECT_EQ(kTestTwoTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_ENABLED, last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_forward_button_state());
}

TEST_F(WebViewTest, NavigationClearsForward) {
  // First navigate somewhere, navigate somewhere else, and go back so we have
  // one item in the forward stack.
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestOneFile));
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestTwoFile));

  web_view()->GoBack();
  StartNestedRunLoopUntil(LOADING_DONE);

  EXPECT_EQ(GetTestFileURL(kTestOneFile).spec(), navigation_url());
  EXPECT_EQ(kTestOneTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_ENABLED,
            last_forward_button_state());

  // Now navigate to a third file. This should clear the forward stack.
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTestThreeFile));

  EXPECT_EQ(GetTestFileURL(kTestThreeFile).spec(), navigation_url());
  EXPECT_EQ(kTestThreeTitle, last_title());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_ENABLED, last_back_button_state());
  EXPECT_EQ(mojom::ButtonState::BUTTON_STATE_DISABLED,
            last_forward_button_state());
}

TEST_F(WebViewTest, Find) {
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTheWordGreenFiveTimes));

  web_view()->Find(1, "Green");
  StartNestedRunLoopUntil(FINAL_FIND_UPATE);
  EXPECT_EQ(1, active_find_match());
  EXPECT_EQ(5, find_count());
}

TEST_F(WebViewTest, FindAcrossIframes) {
  ASSERT_NO_FATAL_FAILURE(NavigateTo(kTwoIframesWithGreen));

  web_view()->Find(1, "Green");
  StartNestedRunLoopUntil(FINAL_FIND_UPATE);
  EXPECT_EQ(13, find_count());
}

}  // namespace web_view
