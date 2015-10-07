// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {

class NavigatorDelegate;
struct NavigationRequestInfo;

// This class keeps track of a single navigation. It is created upon receipt of
// a DidStartProvisionalLoad IPC in a RenderFrameHost. The RenderFrameHost owns
// the newly created NavigationHandleImpl as long as the navigation is ongoing.
// The NavigationHandleImpl in the RenderFrameHost will be reset when the
// navigation stops, that is if one of the following events happen:
//   - The RenderFrameHost receives a DidStartProvisionalLoad IPC for a new
//   navigation (see below for special cases where the DidStartProvisionalLoad
//   message does not indicate the start of a new navigation).
//   - The RenderFrameHost stops loading.
//   - The RenderFrameHost receives a DidDropNavigation IPC.
//
// When the navigation encounters an error, the DidStartProvisionalLoad marking
// the start of the load of the error page will not be considered as marking a
// new navigation. It will not reset the NavigationHandleImpl in the
// RenderFrameHost.
//
// If the navigation needs a cross-site transfer, then the NavigationHandleImpl
// will briefly be held by the RenderFrameHostManager, until a suitable
// RenderFrameHost for the navigation has been found. The ownership of the
// NavigationHandleImpl will then be transferred to the new RenderFrameHost.
// The DidStartProvisionalLoad received by the new RenderFrameHost for the
// transferring navigation will not reset the NavigationHandleImpl, as it does
// not mark the start of a new navigation.
//
// PlzNavigate: the NavigationHandleImpl is created just after creating a new
// NavigationRequest. It is then owned by the NavigationRequest until the
// navigation is ready to commit. The NavigationHandleImpl ownership is then
// transferred to the RenderFrameHost in which the navigation will commit.
//
// When PlzNavigate is enabled, the NavigationHandleImpl will never be reset
// following the receipt of a DidStartProvisionalLoad IPC. There are also no
// transferring navigations. The other causes of NavigationHandleImpl reset in
// the RenderFrameHost still apply.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle {
 public:
  static scoped_ptr<NavigationHandleImpl> Create(
      const GURL& url,
      FrameTreeNode* frame_tree_node);
  ~NavigationHandleImpl() override;

  // NavigationHandle implementation:
  const GURL& GetURL() override;
  bool IsInMainFrame() override;
  bool IsPost() override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  bool IsSamePage() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  void RegisterThrottleForTesting(
      scoped_ptr<NavigationThrottle> navigation_throttle) override;
  NavigationThrottle::ThrottleCheckResult CallWillStartRequestForTesting(
      bool is_post,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillRedirectRequestForTesting(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol) override;

  NavigatorDelegate* GetDelegate() const;

  void set_net_error_code(net::Error net_error_code) {
    net_error_code_ = net_error_code;
  }

  // Returns whether the navigation is currently being transferred from one
  // RenderFrameHost to another. In particular, a DidStartProvisionalLoad IPC
  // for the navigation URL, received in the new RenderFrameHost, should not
  // indicate the start of a new navigation in that case.
  bool is_transferring() const { return is_transferring_; }
  void set_is_transferring(bool is_transferring) {
    is_transferring_ = is_transferring;
  }

  // Called when the URLRequest will start in the network stack.
  NavigationThrottle::ThrottleCheckResult WillStartRequest(
      bool is_post,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol);

  // Called when the URLRequest will be redirected in the network stack.
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol);

  // Called when the navigation was redirected. This will update the |url_| and
  // inform the delegate.
  void DidRedirectNavigation(const GURL& new_url);

  // Called when the navigation is ready to be committed in
  // |render_frame_host|. This will update the |state_| and inform the
  // delegate.
  void ReadyToCommitNavigation(RenderFrameHostImpl* render_frame_host);

  // Called when the navigation was committed in |render_frame_host|. This will
  // update the |state_|.
  void DidCommitNavigation(bool same_page,
                           RenderFrameHostImpl* render_frame_host);

 private:
  // Used to track the state the navigation is currently in.
  enum State {
    INITIAL = 0,
    WILL_SEND_REQUEST,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  NavigationHandleImpl(const GURL& url,
                       FrameTreeNode* frame_tree_node);

  // See NavigationHandle for a description of those member variables.
  GURL url_;
  bool is_post_;
  Referrer sanitized_referrer_;
  bool has_user_gesture_;
  ui::PageTransition transition_;
  bool is_external_protocol_;
  net::Error net_error_code_;
  RenderFrameHostImpl* render_frame_host_;
  bool is_same_page_;

  // The state the navigation is in.
  State state_;

  // Whether the navigation is in the middle of a transfer. Set to false when
  // the DidStartProvisionalLoad is received from the new renderer.
  bool is_transferring_;

  // The FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node_;

  // A list of Throttles registered for this navigation.
  ScopedVector<NavigationThrottle> throttles_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
