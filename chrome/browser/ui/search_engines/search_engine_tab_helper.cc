// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_fetcher.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/url_fetcher.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SearchEngineTabHelper);

namespace {

// Returns true if the entry's transition type is FORM_SUBMIT.
bool IsFormSubmit(const NavigationEntry* entry) {
  return (ui::PageTransitionStripQualifier(entry->GetTransitionType()) ==
          ui::PAGE_TRANSITION_FORM_SUBMIT);
}

base::string16 GenerateKeywordFromNavigationEntry(
    const NavigationEntry* entry,
    const std::string& accept_languages) {
  // Don't autogenerate keywords for pages that are the result of form
  // submissions.
  if (IsFormSubmit(entry))
    return base::string16();

  // We want to use the user typed URL if available since that represents what
  // the user typed to get here, and fall back on the regular URL if not.
  GURL url = entry->GetUserTypedURL();
  if (!url.is_valid()) {
    url = entry->GetURL();
    if (!url.is_valid())
      return base::string16();
  }

  // Don't autogenerate keywords for referrers that
  // a) are anything other than HTTP/HTTPS or
  // b) have a path.
  //
  // If we relax the path constraint, we need to be sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme)) ||
      (url.path().length() > 1)) {
    return base::string16();
  }

  return TemplateURL::GenerateKeyword(url, accept_languages);
}

void AssociateURLFetcherWithWebContents(content::WebContents* web_contents,
                                        net::URLFetcher* url_fetcher) {
  content::AssociateURLFetcherWithRenderFrame(
      url_fetcher,
      web_contents->GetURL(),
      web_contents->GetRenderProcessHost()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());
}

}  // namespace

SearchEngineTabHelper::~SearchEngineTabHelper() {
}

void SearchEngineTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& params) {
  GenerateKeywordIfNecessary(params);
}

bool SearchEngineTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchEngineTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PageHasOSDD, OnPageHasOSDD)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool SearchEngineTabHelper::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  return OnMessageReceived(message);
}

SearchEngineTabHelper::SearchEngineTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(web_contents);
}

void SearchEngineTabHelper::OnPageHasOSDD(
    const GURL& page_url,
    const GURL& osdd_url,
    const search_provider::OSDDType& msg_provider_type) {
  // Checks to see if we should generate a keyword based on the OSDD, and if
  // necessary uses TemplateURLFetcher to download the OSDD and create a
  // keyword.

  // Make sure that the page is the current page and other basic checks.
  // When |page_url| has file: scheme, this method doesn't work because of
  // http://b/issue?id=863583. For that reason, this doesn't check and allow
  // urls referring to osdd urls with same schemes.
  if (!osdd_url.is_valid() || !osdd_url.SchemeIsHTTPOrHTTPS())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (page_url != web_contents()->GetLastCommittedURL() ||
      !TemplateURLFetcherFactory::GetForProfile(profile) ||
      profile->IsOffTheRecord())
    return;

  TemplateURLFetcher::ProviderType provider_type =
      (msg_provider_type == search_provider::AUTODETECTED_PROVIDER) ?
          TemplateURLFetcher::AUTODETECTED_PROVIDER :
          TemplateURLFetcher::EXPLICIT_PROVIDER;

  // If the current page is a form submit, find the last page that was not a
  // form submit and use its url to generate the keyword from.
  const NavigationController& controller = web_contents()->GetController();
  const NavigationEntry* entry = controller.GetLastCommittedEntry();
  for (int index = controller.GetLastCommittedEntryIndex();
       (index > 0) && IsFormSubmit(entry);
       entry = controller.GetEntryAtIndex(index))
    --index;
  if (!entry || IsFormSubmit(entry))
    return;

  // Autogenerate a keyword for the autodetected case; in the other cases we'll
  // generate a keyword later after fetching the OSDD.
  base::string16 keyword;
  if (provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER) {
    keyword = GenerateKeywordFromNavigationEntry(
        entry, profile->GetPrefs()->GetString(prefs::kAcceptLanguages));
    if (keyword.empty())
      return;
  }

  // Download the OpenSearch description document. If this is successful, a
  // new keyword will be created when done.
  TemplateURLFetcherFactory::GetForProfile(profile)->ScheduleDownload(
      keyword, osdd_url, entry->GetFavicon().url,
      base::Bind(&AssociateURLFetcherWithWebContents, web_contents()),
      base::Bind(&SearchEngineTabHelper::OnDownloadedOSDD,
                 weak_ptr_factory_.GetWeakPtr()),
      provider_type);
}

void SearchEngineTabHelper::OnDownloadedOSDD(
    scoped_ptr<TemplateURL> template_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  delegate_->ConfirmAddSearchProvider(template_url.release(), profile);
}

void SearchEngineTabHelper::GenerateKeywordIfNecessary(
    const content::FrameNavigateParams& params) {
  if (!params.searchable_form_url.is_valid())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  const NavigationController& controller = web_contents()->GetController();
  int last_index = controller.GetLastCommittedEntryIndex();
  // When there was no previous page, the last index will be 0. This is
  // normally due to a form submit that opened in a new tab.
  // TODO(brettw) bug 916126: we should support keywords when form submits
  //              happen in new tabs.
  if (last_index <= 0)
    return;

  base::string16 keyword(GenerateKeywordFromNavigationEntry(
      controller.GetEntryAtIndex(last_index - 1),
      profile->GetPrefs()->GetString(prefs::kAcceptLanguages)));
  if (keyword.empty())
    return;

  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!url_service)
    return;

  if (!url_service->loaded()) {
    url_service->Load();
    return;
  }

  TemplateURL* current_url;
  GURL url = params.searchable_form_url;
  if (!url_service->CanAddAutogeneratedKeyword(keyword, url, &current_url))
    return;

  if (current_url) {
    if (current_url->originating_url().is_valid()) {
      // The existing keyword was generated from an OpenSearch description
      // document, don't regenerate.
      return;
    }
    url_service->Remove(current_url);
  }

  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL(url.spec());
  DCHECK(controller.GetLastCommittedEntry());
  const GURL& current_favicon =
      controller.GetLastCommittedEntry()->GetFavicon().url;
  // If the favicon url isn't valid, it means there really isn't a favicon, or
  // the favicon url wasn't obtained before the load started. This assumes the
  // latter.
  // TODO(sky): Need a way to set the favicon that doesn't involve generating
  // its url.
  data.favicon_url = current_favicon.is_valid() ?
      current_favicon : TemplateURL::GenerateFaviconURL(params.referrer.url);
  data.safe_for_autoreplace = true;
  data.input_encodings.push_back(params.searchable_form_encoding);
  url_service->Add(new TemplateURL(data));
}
