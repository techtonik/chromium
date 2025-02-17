// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/bind.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon/ios/favicon_url_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/gfx/image/image.h"

DEFINE_WEB_STATE_USER_DATA_KEY(favicon::WebFaviconDriver);

namespace favicon {

// static
void WebFaviconDriver::CreateForWebState(
    web::WebState* web_state,
    FaviconService* favicon_service,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(UserDataKey(),
                         new WebFaviconDriver(web_state, favicon_service,
                                              history_service, bookmark_model));
}

void WebFaviconDriver::FetchFavicon(const GURL& url) {
  fetch_favicon_url_ = url;
  FaviconDriverImpl::FetchFavicon(url);
}

gfx::Image WebFaviconDriver::GetFavicon() const {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().image : gfx::Image();
}

bool WebFaviconDriver::FaviconIsValid() const {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().valid : false;
}

int WebFaviconDriver::StartDownload(const GURL& url, int max_image_size) {
  if (WasUnableToDownloadFavicon(url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << url;
    return 0;
  }

  return web_state()->DownloadImage(
      url, true, max_image_size, false,
      base::Bind(&FaviconDriverImpl::DidDownloadFavicon,
                 base::Unretained(this)));
}

bool WebFaviconDriver::IsOffTheRecord() {
  DCHECK(web_state());
  return web_state()->GetBrowserState()->IsOffTheRecord();
}

GURL WebFaviconDriver::GetActiveURL() {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  return item ? item->GetURL() : GURL();
}

bool WebFaviconDriver::GetActiveFaviconValidity() {
  return !ActiveURLChangedSinceFetchFavicon() && GetFaviconStatus().valid;
}

void WebFaviconDriver::SetActiveFaviconValidity(bool validity) {
  GetFaviconStatus().valid = validity;
}

GURL WebFaviconDriver::GetActiveFaviconURL() {
  return ActiveURLChangedSinceFetchFavicon() ? GURL() : GetFaviconStatus().url;
}

void WebFaviconDriver::SetActiveFaviconURL(const GURL& url) {
  GetFaviconStatus().url = url;
}

void WebFaviconDriver::SetActiveFaviconImage(const gfx::Image& image) {
  GetFaviconStatus().image = image;
}

bool WebFaviconDriver::ActiveURLChangedSinceFetchFavicon() {
  // On iOS the active URL can change in between calls to FetchFavicon(). For
  // instance, FetchFavicon() is not synchronously called when the active URL
  // changes as a result of CRWSessionController::goToEntry().
  // TODO(stuartmorgan): Remove this once iOS always triggers favicon fetches
  // synchronously after active URL changes.
  return GetActiveURL() != fetch_favicon_url_;
}

web::FaviconStatus& WebFaviconDriver::GetFaviconStatus() {
  DCHECK(!ActiveURLChangedSinceFetchFavicon());
  return web_state()->GetNavigationManager()->GetVisibleItem()->GetFavicon();
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model)
    : web::WebStateObserver(web_state),
      FaviconDriverImpl(favicon_service, history_service, bookmark_model) {
}

WebFaviconDriver::~WebFaviconDriver() {
}

void WebFaviconDriver::FaviconUrlUpdated(
    const std::vector<web::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  OnUpdateFaviconURL(GetActiveURL(), FaviconURLsFromWebFaviconURLs(candidates));
}

}  // namespace favicon
