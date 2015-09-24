// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/popular_sites.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/net/file_downloader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/google/core/browser/google_util.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kPopularSitesURLFormat[] = "https://www.gstatic.com/chrome/ntp/%s";
const char kPopularSitesServerFilenameFormat[] = "suggested_sites_%s_%s.json";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";
const char kPopularSitesDefaultVersion[] = "2";
const char kPopularSitesLocalFilename[] = "suggested_sites.json";


// Find out the country code of the user by using the Google country code if
// Google is the default search engine set. Fallback to a default if we can't
// make an educated guess.
std::string GetCountryCode(Profile* profile) {
  DCHECK(profile);

  const TemplateURLService* template_url_service =
     TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // It's possible to not have a default provider in the case that the default
  // search engine is defined by policy.
  if (!default_provider)
      return kPopularSitesDefaultCountryCode;

  bool is_google_search_engine = TemplateURLPrepopulateData::GetEngineType(
      *default_provider, template_url_service->search_terms_data()) ==
          SearchEngineType::SEARCH_ENGINE_GOOGLE;

  if (!is_google_search_engine)
    return kPopularSitesDefaultCountryCode;

  GURL search_url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data());

  std::string country_code =
      base::ToUpperASCII(google_util::GetGoogleCountryCode(search_url));

  return country_code;
}

std::string GetPopularSitesServerFilename(
    Profile* profile,
    const std::string& override_country,
    const std::string& override_version,
    const std::string& override_filename) {
  if (!override_filename.empty())
    return override_filename;

  std::string country = !override_country.empty() ? override_country
                                                  : GetCountryCode(profile);
  std::string version = !override_version.empty() ? override_version
                                                  : kPopularSitesDefaultVersion;
  return base::StringPrintf(kPopularSitesServerFilenameFormat,
                            country.c_str(), version.c_str());
}

GURL GetPopularSitesURL(Profile* profile,
                        const std::string& override_country,
                        const std::string& override_version,
                        const std::string& override_filename) {
  return GURL(base::StringPrintf(kPopularSitesURLFormat,
      GetPopularSitesServerFilename(profile,
                                    override_country,
                                    override_version,
                                    override_filename).c_str()));
}

base::FilePath GetPopularSitesPath() {
  base::FilePath dir;
  PathService::Get(chrome::DIR_USER_DATA, &dir);
  return dir.AppendASCII(kPopularSitesLocalFilename);
}

scoped_ptr<std::vector<PopularSites::Site>> ReadAndParseJsonFile(
    const base::FilePath& path) {
  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    DLOG(WARNING) << "Failed reading file";
    return nullptr;
  }

  scoped_ptr<base::Value> value =
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
  base::ListValue* list;
  if (!value || !value->GetAsList(&list)) {
    DLOG(WARNING) << "Failed parsing json";
    return nullptr;
  }

  scoped_ptr<std::vector<PopularSites::Site>> sites(
      new std::vector<PopularSites::Site>);
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::DictionaryValue* item;
    if (!list->GetDictionary(i, &item))
      continue;
    base::string16 title;
    std::string url;
    if (!item->GetString("title", &title) || !item->GetString("url", &url))
      continue;
    std::string favicon_url;
    item->GetString("favicon_url", &favicon_url);
    std::string thumbnail_url;
    item->GetString("thumbnail_url", &thumbnail_url);

    sites->push_back(PopularSites::Site(title, GURL(url), GURL(favicon_url),
                                        GURL(thumbnail_url)));
  }

  return sites.Pass();
}

}  // namespace

PopularSites::Site::Site(const base::string16& title,
                         const GURL& url,
                         const GURL& favicon_url,
                         const GURL& thumbnail_url)
    : title(title),
      url(url),
      favicon_url(favicon_url),
      thumbnail_url(thumbnail_url) {}

PopularSites::Site::~Site() {}

PopularSites::PopularSites(Profile* profile,
                           const std::string& override_country,
                           const std::string& override_version,
                           const std::string& override_filename,
                           bool force_download,
                           const FinishedCallback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  // Re-download the file once on every Chrome startup, but use the cached
  // local file afterwards.
  static bool first_time = true;
  FetchPopularSites(GetPopularSitesURL(profile, override_country,
                                       override_version, override_filename),
                    profile->GetRequestContext(), first_time || force_download);
  first_time = false;
}

PopularSites::PopularSites(Profile* profile,
                           const GURL& url,
                           const FinishedCallback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  FetchPopularSites(url, profile->GetRequestContext(), true);
}

PopularSites::~PopularSites() {}

void PopularSites::FetchPopularSites(
    const GURL& url,
    net::URLRequestContextGetter* request_context,
    bool force_download) {
  base::FilePath path = GetPopularSitesPath();
  downloader_.reset(new FileDownloader(
      url, path, force_download, request_context,
      base::Bind(&PopularSites::OnDownloadDone, base::Unretained(this), path)));
}

void PopularSites::OnDownloadDone(const base::FilePath& path, bool success) {
  if (success) {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN).get(),
        FROM_HERE,
        base::Bind(&ReadAndParseJsonFile, path),
        base::Bind(&PopularSites::OnJsonParsed,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    DLOG(WARNING) << "Download failed";
    callback_.Run(false);
  }

  downloader_.reset();
}

void PopularSites::OnJsonParsed(scoped_ptr<std::vector<Site>> sites) {
  if (sites)
    sites_.swap(*sites);
  else
    sites_.clear();
  callback_.Run(!!sites);
}
