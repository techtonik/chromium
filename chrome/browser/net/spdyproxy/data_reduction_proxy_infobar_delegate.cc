// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_infobar_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void DataReductionProxyInfoBarDelegate::Create(
    content::WebContents* web_contents, const std::string& link_url) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(
      DataReductionProxyInfoBarDelegate::CreateInfoBar(
          infobar_service,
          scoped_ptr<DataReductionProxyInfoBarDelegate>(
              new DataReductionProxyInfoBarDelegate(link_url))));
}

DataReductionProxyInfoBarDelegate::~DataReductionProxyInfoBarDelegate() {
}

DataReductionProxyInfoBarDelegate::DataReductionProxyInfoBarDelegate(
    const std::string& link_url)
    : ConfirmInfoBarDelegate(),
      link_url_(link_url) {
}

bool DataReductionProxyInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

base::string16 DataReductionProxyInfoBarDelegate::GetMessageText() const {
  return base::string16();
}

int DataReductionProxyInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

GURL DataReductionProxyInfoBarDelegate::GetLinkURL() const {
  return GURL(link_url_);
}

bool DataReductionProxyInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  ConfirmInfoBarDelegate::LinkClicked(disposition);
  return true;
}
