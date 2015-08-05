// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_android.h"

#include "base/logging.h"

namespace media_router {

MediaRouterAndroid::MediaRouterAndroid(content::BrowserContext*) {
}

MediaRouterAndroid::~MediaRouterAndroid() {
}

void MediaRouterAndroid::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::JoinRoute(
    const MediaSource::Id& source,
    const std::string& presentation_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::CloseRoute(const MediaRoute::Id& route_id) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    scoped_ptr<std::vector<uint8>> data,
    const SendRouteMessageCallback& callback) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::ListenForRouteMessages(
    const std::vector<MediaRoute::Id>& route_ids,
    const PresentationSessionMessageCallback& message_cb) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::ClearIssue(const Issue::Id& issue_id) {
  NOTIMPLEMENTED();
}

void MediaRouterAndroid::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::RegisterIssuesObserver(IssuesObserver* observer) {
  NOTIMPLEMENTED();
}
void MediaRouterAndroid::UnregisterIssuesObserver(IssuesObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace media_router
