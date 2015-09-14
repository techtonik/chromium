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
const char kPopularSitesFilenameFormat[] = "suggested_sites_%s_2.json";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";


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

std::string GetPopularSitesFilename(Profile* profile,
                                    const std::string& filename) {
  if (!filename.empty())
    return filename;

  return base::StringPrintf(kPopularSitesFilenameFormat,
                            GetCountryCode(profile).c_str());
}

base::FilePath GetPopularSitesPath(Profile* profile,
                                   const std::string& filename) {
  base::FilePath dir;
  PathService::Get(chrome::DIR_USER_DATA, &dir);
  return dir.AppendASCII(GetPopularSitesFilename(profile, filename));
}

GURL GetPopularSitesURL(Profile* profile, const std::string& filename) {
  return GURL(base::StringPrintf(kPopularSitesURLFormat,
      GetPopularSitesFilename(profile, filename).c_str()));
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
                           const std::string& filename,
                           net::URLRequestContextGetter* request_context,
                           const FinishedCallback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  base::FilePath path = GetPopularSitesPath(profile, filename);
  downloader_.reset(new FileDownloader(
      GetPopularSitesURL(profile, filename), path, request_context,
      base::Bind(&PopularSites::OnDownloadDone, base::Unretained(this), path)));
}

PopularSites::~PopularSites() {}

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
