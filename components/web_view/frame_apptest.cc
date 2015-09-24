// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "components/web_view/frame.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/test_frame_tree_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

using mus::View;
using mus::ViewTreeConnection;

namespace web_view {

namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

bool DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

void QuitRunLoop() {
  current_run_loop->Quit();
  current_run_loop = nullptr;
}

}  // namespace

void OnGotIdCallback(base::RunLoop* run_loop) {
  run_loop->Quit();
}

// Creates a new FrameConnection. This runs a nested message loop until the
// content handler id is obtained.
scoped_ptr<FrameConnection> CreateFrameConnection(mojo::ApplicationImpl* app) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(app->url());
  base::RunLoop run_loop;
  frame_connection->Init(app, request.Pass(),
                         base::Bind(&OnGotIdCallback, &run_loop));
  run_loop.Run();
  return frame_connection;
}

class TestFrameClient : public mojom::FrameClient {
 public:
  TestFrameClient()
      : connect_count_(0), last_dispatch_load_event_frame_id_(0) {}
  ~TestFrameClient() override {}

  int connect_count() const { return connect_count_; }

  mojo::Array<mojom::FrameDataPtr> connect_frames() {
    return connect_frames_.Pass();
  }

  mojo::Array<mojom::FrameDataPtr> adds() { return adds_.Pass(); }

  // Sets a callback to run once OnConnect() is received.
  void set_on_connect_callback(const base::Closure& closure) {
    on_connect_callback_ = closure;
  }

  void set_on_loading_state_changed_callback(const base::Closure& closure) {
    on_loading_state_changed_callback_ = closure;
  }

  void set_on_dispatch_load_event_callback(const base::Closure& closure) {
    on_dispatch_load_event_callback_ = closure;
  }

  mojom::Frame* server_frame() { return server_frame_.get(); }

  mojo::InterfaceRequest<mojom::Frame> GetServerFrameRequest() {
    return GetProxy(&server_frame_);
  }

  void last_loading_state_changed_notification(uint32_t* frame_id,
                                               bool* loading) const {
    *frame_id = last_loading_state_changed_notification_.frame_id;
    *loading = last_loading_state_changed_notification_.loading;
  }

  uint32_t last_dispatch_load_event_frame_id() const {
    return last_dispatch_load_event_frame_id_;
  }

  // mojom::FrameClient:
  void OnConnect(mojom::FramePtr frame,
                 uint32_t change_id,
                 uint32_t view_id,
                 mojom::ViewConnectType view_connect_type,
                 mojo::Array<mojom::FrameDataPtr> frames,
                 const OnConnectCallback& callback) override {
    connect_count_++;
    connect_frames_ = frames.Pass();
    if (frame)
      server_frame_ = frame.Pass();
    callback.Run();
    if (!on_connect_callback_.is_null())
      on_connect_callback_.Run();
  }
  void OnFrameAdded(uint32_t change_id, mojom::FrameDataPtr frame) override {
    adds_.push_back(frame.Pass());
  }
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override {}
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_data) override {}
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          mojom::HTMLMessageEventPtr event) override {}
  void OnWillNavigate(const OnWillNavigateCallback& callback) override {
    callback.Run();
  }
  void OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) override {
    last_loading_state_changed_notification_.frame_id = frame_id;
    last_loading_state_changed_notification_.loading = loading;

    if (!on_loading_state_changed_callback_.is_null())
      on_loading_state_changed_callback_.Run();
  }
  void OnDispatchFrameLoadEvent(uint32_t frame_id) override {
    last_dispatch_load_event_frame_id_ = frame_id;

    if (!on_dispatch_load_event_callback_.is_null())
      on_dispatch_load_event_callback_.Run();
  }

 private:
  struct LoadingStateChangedNotification {
    LoadingStateChangedNotification() : frame_id(0), loading(false) {}
    ~LoadingStateChangedNotification() {}

    uint32_t frame_id;
    bool loading;
  };

  int connect_count_;
  mojo::Array<mojom::FrameDataPtr> connect_frames_;
  mojom::FramePtr server_frame_;
  mojo::Array<mojom::FrameDataPtr> adds_;
  base::Closure on_connect_callback_;
  base::Closure on_loading_state_changed_callback_;
  base::Closure on_dispatch_load_event_callback_;
  LoadingStateChangedNotification last_loading_state_changed_notification_;
  uint32_t last_dispatch_load_event_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameClient);
};

class FrameTest;

// ViewAndFrame maintains the View and TestFrameClient associated with
// a single FrameClient. In other words this maintains the data structures
// needed to represent a client side frame. To obtain one use
// FrameTest::WaitForViewAndFrame().
class ViewAndFrame : public mus::ViewTreeDelegate {
 public:
  ~ViewAndFrame() override {
    if (view_)
      delete view_->connection();
  }

  // The View associated with the frame.
  mus::View* view() { return view_; }
  TestFrameClient* test_frame_client() { return &test_frame_tree_client_; }
  mojom::Frame* server_frame() {
    return test_frame_tree_client_.server_frame();
  }

 private:
  friend class FrameTest;

  ViewAndFrame()
      : view_(nullptr), frame_client_binding_(&test_frame_tree_client_) {}

  void set_view(View* view) { view_ = view; }

  // Runs a message loop until the view and frame data have been received.
  void WaitForViewAndFrame() { run_loop_.Run(); }

  mojo::InterfaceRequest<mojom::Frame> GetServerFrameRequest() {
    return test_frame_tree_client_.GetServerFrameRequest();
  }

  mojom::FrameClientPtr GetFrameClientPtr() {
    mojom::FrameClientPtr client_ptr;
    frame_client_binding_.Bind(GetProxy(&client_ptr));
    return client_ptr.Pass();
  }

  void Bind(mojo::InterfaceRequest<mojom::FrameClient> request) {
    ASSERT_FALSE(frame_client_binding_.is_bound());
    test_frame_tree_client_.set_on_connect_callback(
        base::Bind(&ViewAndFrame::OnGotConnect, base::Unretained(this)));
    frame_client_binding_.Bind(request.Pass());
  }

  void OnGotConnect() { QuitRunLoopIfNecessary(); }

  void QuitRunLoopIfNecessary() {
    if (view_ && test_frame_tree_client_.connect_count())
      run_loop_.Quit();
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    view_ = root;
    QuitRunLoopIfNecessary();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {
    view_ = nullptr;
  }

  mus::View* view_;
  base::RunLoop run_loop_;
  TestFrameClient test_frame_tree_client_;
  mojo::Binding<mojom::FrameClient> frame_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndFrame);
};

class FrameTest : public mojo::test::ApplicationTestBase,
                  public mojo::ApplicationDelegate,
                  public mus::ViewTreeDelegate,
                  public mojo::InterfaceFactory<mojo::ViewTreeClient>,
                  public mojo::InterfaceFactory<mojom::FrameClient> {
 public:
  FrameTest() : most_recent_connection_(nullptr), window_manager_(nullptr) {}

  ViewTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

 protected:
  ViewTreeConnection* window_manager() { return window_manager_; }
  TestFrameTreeDelegate* frame_tree_delegate() {
    return frame_tree_delegate_.get();
  }
  FrameTree* frame_tree() { return frame_tree_.get(); }
  ViewAndFrame* root_view_and_frame() { return root_view_and_frame_.get(); }

  scoped_ptr<ViewAndFrame> NavigateFrame(ViewAndFrame* view_and_frame) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(application_impl()->url());
    view_and_frame->server_frame()->RequestNavigate(
        mojom::NAVIGATION_TARGET_TYPE_EXISTING_FRAME,
        view_and_frame->view()->id(), request.Pass());
    return WaitForViewAndFrame();
  }

  // Creates a new shared frame as a child of |parent|.
  scoped_ptr<ViewAndFrame> CreateChildViewAndFrame(ViewAndFrame* parent) {
    mus::View* child_frame_view = parent->view()->connection()->CreateView();
    parent->view()->AddChild(child_frame_view);

    scoped_ptr<ViewAndFrame> view_and_frame(new ViewAndFrame);
    view_and_frame->set_view(child_frame_view);

    mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
    client_properties.mark_non_null();
    parent->server_frame()->OnCreatedFrame(
        view_and_frame->GetServerFrameRequest(),
        view_and_frame->GetFrameClientPtr(), child_frame_view->id(),
        client_properties.Pass());
    frame_tree_delegate()->WaitForCreateFrame();
    return HasFatalFailure() ? nullptr : view_and_frame.Pass();
  }

  // Runs a message loop until the data necessary to represent to a client side
  // frame has been obtained.
  scoped_ptr<ViewAndFrame> WaitForViewAndFrame() {
    DCHECK(!view_and_frame_);
    view_and_frame_.reset(new ViewAndFrame);
    view_and_frame_->WaitForViewAndFrame();
    return view_and_frame_.Pass();
  }

 private:
  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::ViewTreeClient>(this);
    connection->AddService<mojom::FrameClient>(this);
    return true;
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    most_recent_connection_ = root->connection();
    QuitRunLoop();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {}

  // Overridden from testing::Test:
  void SetUp() override {
    ApplicationTestBase::SetUp();

    mus::CreateSingleViewTreeHost(application_impl(), this, &host_);

    ASSERT_TRUE(DoRunLoopWithTimeout());
    std::swap(window_manager_, most_recent_connection_);

    // Creates a FrameTree, which creates a single frame. Wait for the
    // FrameClient to be connected to.
    frame_tree_delegate_.reset(new TestFrameTreeDelegate(application_impl()));
    scoped_ptr<FrameConnection> frame_connection =
        CreateFrameConnection(application_impl());
    mojom::FrameClient* frame_client = frame_connection->frame_client();
    mojo::ViewTreeClientPtr view_tree_client =
        frame_connection->GetViewTreeClient();
    mus::View* frame_root_view = window_manager()->CreateView();
    window_manager()->GetRoot()->AddChild(frame_root_view);
    frame_tree_.reset(
        new FrameTree(0u, frame_root_view, view_tree_client.Pass(),
                      frame_tree_delegate_.get(), frame_client,
                      frame_connection.Pass(), Frame::ClientPropertyMap()));
    root_view_and_frame_ = WaitForViewAndFrame();
  }

  // Overridden from testing::Test:
  void TearDown() override {
    root_view_and_frame_.reset();
    frame_tree_.reset();
    frame_tree_delegate_.reset();
    ApplicationTestBase::TearDown();
  }

  // Overridden from mojo::InterfaceFactory<mojo::ViewTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ViewTreeClient> request) override {
    if (view_and_frame_) {
      mus::ViewTreeConnection::Create(view_and_frame_.get(), request.Pass());
    } else {
      mus::ViewTreeConnection::Create(this, request.Pass());
    }
  }

  // Overridden from mojo::InterfaceFactory<mojom::FrameClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::FrameClient> request) override {
    ASSERT_TRUE(view_and_frame_);
    view_and_frame_->Bind(request.Pass());
  }

  scoped_ptr<TestFrameTreeDelegate> frame_tree_delegate_;
  scoped_ptr<FrameTree> frame_tree_;
  scoped_ptr<ViewAndFrame> root_view_and_frame_;

  mojo::ViewTreeHostPtr host_;

  // Used to receive the most recent view manager loaded by an embed action.
  ViewTreeConnection* most_recent_connection_;
  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewTreeConnection* window_manager_;

  scoped_ptr<ViewAndFrame> view_and_frame_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FrameTest);
};

// Verifies the FrameData supplied to the root FrameClient::OnConnect().
TEST_F(FrameTest, RootFrameClientConnectData) {
  mojo::Array<mojom::FrameDataPtr> frames =
      root_view_and_frame()->test_frame_client()->connect_frames();
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(root_view_and_frame()->view()->id(), frames[0]->frame_id);
  EXPECT_EQ(0u, frames[0]->parent_id);
}

// Verifies the FrameData supplied to a child FrameClient::OnConnect().
TEST_F(FrameTest, ChildFrameClientConnectData) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  ASSERT_TRUE(child_view_and_frame);
  // Initially created child frames don't get OnConnect().
  EXPECT_EQ(0, child_view_and_frame->test_frame_client()->connect_count());

  scoped_ptr<ViewAndFrame> navigated_child_view_and_frame =
      NavigateFrame(child_view_and_frame.get()).Pass();

  mojo::Array<mojom::FrameDataPtr> frames_in_child =
      navigated_child_view_and_frame->test_frame_client()->connect_frames();
  EXPECT_EQ(child_view_and_frame->view()->id(),
            navigated_child_view_and_frame->view()->id());
  // We expect 2 frames. One for the root, one for the child.
  ASSERT_EQ(2u, frames_in_child.size());
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[0]->frame_id);
  EXPECT_EQ(0u, frames_in_child[0]->parent_id);
  EXPECT_EQ(navigated_child_view_and_frame->view()->id(),
            frames_in_child[1]->frame_id);
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[1]->parent_id);
}

TEST_F(FrameTest, OnViewEmbeddedInFrameDisconnected) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  ASSERT_TRUE(child_view_and_frame);

  scoped_ptr<ViewAndFrame> navigated_child_view_and_frame =
      NavigateFrame(child_view_and_frame.get()).Pass();

  // Delete the ViewTreeConnection for the child, which should trigger
  // notification.
  delete navigated_child_view_and_frame->view()->connection();
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
  ASSERT_NO_FATAL_FAILURE(frame_tree_delegate()->WaitForFrameDisconnected(
      frame_tree()->root()->children()[0]));
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
}

TEST_F(FrameTest, NotifyRemoteParentWithLoadingState) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  uint32_t child_frame_id = child_view_and_frame->view()->id();

  {
    base::RunLoop run_loop;
    root_view_and_frame()
        ->test_frame_client()
        ->set_on_loading_state_changed_callback(run_loop.QuitClosure());

    child_view_and_frame->server_frame()->LoadingStateChanged(true, .5);

    run_loop.Run();

    uint32_t frame_id = 0;
    bool loading = false;
    root_view_and_frame()
        ->test_frame_client()
        ->last_loading_state_changed_notification(&frame_id, &loading);
    EXPECT_EQ(child_frame_id, frame_id);
    EXPECT_TRUE(loading);
  }
  {
    base::RunLoop run_loop;
    root_view_and_frame()
        ->test_frame_client()
        ->set_on_loading_state_changed_callback(run_loop.QuitClosure());

    ASSERT_TRUE(child_view_and_frame);
    ASSERT_TRUE(child_view_and_frame->server_frame());

    child_view_and_frame->server_frame()->LoadingStateChanged(false, 1);

    run_loop.Run();

    uint32_t frame_id = 0;
    bool loading = false;
    root_view_and_frame()
        ->test_frame_client()
        ->last_loading_state_changed_notification(&frame_id, &loading);
    EXPECT_EQ(child_frame_id, frame_id);
    EXPECT_FALSE(loading);
  }
}

TEST_F(FrameTest, NotifyRemoteParentWithLoadEvent) {
  scoped_ptr<ViewAndFrame> child_view_and_frame(
      CreateChildViewAndFrame(root_view_and_frame()));
  uint32_t child_frame_id = child_view_and_frame->view()->id();

  base::RunLoop run_loop;
  root_view_and_frame()
      ->test_frame_client()
      ->set_on_dispatch_load_event_callback(run_loop.QuitClosure());

  child_view_and_frame->server_frame()->DispatchLoadEventToParent();

  run_loop.Run();

  uint32_t frame_id = root_view_and_frame()
                          ->test_frame_client()
                          ->last_dispatch_load_event_frame_id();
  EXPECT_EQ(child_frame_id, frame_id);
}
}  // namespace web_view
