// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

base::FilePath::CharType kExternalExtensionJson[] =
    FILE_PATH_LITERAL("external_extensions.json");

std::set<base::FilePath> GetPrefsCandidateFilesFromFolder(
      const base::FilePath& external_extension_search_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::set<base::FilePath> external_extension_paths;

  if (!base::PathExists(external_extension_search_path)) {
    // Does not have to exist.
    return external_extension_paths;
  }

  base::FileEnumerator json_files(
      external_extension_search_path,
      false,  // Recursive.
      base::FileEnumerator::FILES);
#if defined(OS_WIN)
  base::FilePath::StringType extension = base::UTF8ToWide(".json");
#elif defined(OS_POSIX)
  base::FilePath::StringType extension(".json");
#endif
  do {
    base::FilePath file = json_files.Next();
    if (file.BaseName().value() == kExternalExtensionJson)
      continue;  // Already taken care of elsewhere.
    if (file.empty())
      break;
    if (file.MatchesExtension(extension)) {
      external_extension_paths.insert(file.BaseName());
    } else {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
    }
  } while (true);

  return external_extension_paths;
}

// Extracts extension information from a json file serialized by |serializer|.
// |path| is only used for informational purposes (outputted when an error
// occurs). An empty dictionary is returned in case of failure (e.g. invalid
// path or json content).
// Caller takes ownership of the returned dictionary.
base::DictionaryValue* ExtractExtensionPrefs(
    base::ValueDeserializer* deserializer,
    const base::FilePath& path) {
  std::string error_msg;
  base::Value* extensions = deserializer->Deserialize(NULL, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg
                 << " in file " << path.value() << ".";
    return new base::DictionaryValue;
  }

  base::DictionaryValue* ext_dictionary = NULL;
  if (extensions->GetAsDictionary(&ext_dictionary))
    return ext_dictionary;

  LOG(WARNING) << "Expected a JSON dictionary in file "
               << path.value() << ".";
  return new base::DictionaryValue;
}

}  // namespace

namespace extensions {

ExternalPrefLoader::ExternalPrefLoader(int base_path_id,
                                       Options options,
                                       Profile* profile)
    : base_path_id_(base_path_id),
      options_(options),
      profile_(profile),
      syncable_pref_observer_(this) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExternalPrefLoader::~ExternalPrefLoader() {
}

const base::FilePath ExternalPrefLoader::GetBaseCrxFilePath() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |base_path_| was set in LoadOnFileThread().
  return base_path_;
}

void ExternalPrefLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if ((options_ & DELAY_LOAD_UNTIL_PRIORITY_SYNC) &&
      (profile_ && profile_->IsSyncAllowed())) {
    if (!PostLoadIfPrioritySyncReady()) {
      DCHECK(profile_);
      syncable_prefs::PrefServiceSyncable* prefs =
          PrefServiceSyncableFromProfile(profile_);
      DCHECK(prefs);
      syncable_pref_observer_.Add(prefs);
      ProfileSyncService* service =
          ProfileSyncServiceFactory::GetForProfile(profile_);
      DCHECK(service);
      if (service->CanSyncStart() &&
          (service->HasSyncSetupCompleted() ||
           browser_defaults::kSyncAutoStarts)) {
        service->AddObserver(this);
      } else {
        PostLoadAndRemoveObservers();
      }
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ExternalPrefLoader::LoadOnFileThread, this));
  }
}

void ExternalPrefLoader::OnIsSyncingChanged() {
  PostLoadIfPrioritySyncReady();
}

void ExternalPrefLoader::OnStateChanged() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  DCHECK(service);
  if (!service->CanSyncStart()) {
    PostLoadAndRemoveObservers();
  }
}

bool ExternalPrefLoader::PostLoadIfPrioritySyncReady() {
  DCHECK(options_ & DELAY_LOAD_UNTIL_PRIORITY_SYNC);
  DCHECK(profile_);

  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  DCHECK(prefs);
  if (prefs->IsPrioritySyncing()) {
    PostLoadAndRemoveObservers();
    return true;
  }

  return false;
}

void ExternalPrefLoader::PostLoadAndRemoveObservers() {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  DCHECK(prefs);
  syncable_pref_observer_.Remove(prefs);

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  DCHECK(service);
  service->RemoveObserver(this);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ExternalPrefLoader::LoadOnFileThread, this));
}

void ExternalPrefLoader::LoadOnFileThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);

  // TODO(skerner): Some values of base_path_id_ will cause
  // PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/70402 .
  if (PathService::Get(base_path_id_, &base_path_)) {
    ReadExternalExtensionPrefFile(prefs.get());

    if (!prefs->empty())
      LOG(WARNING) << "You are using an old-style extension deployment method "
                      "(external_extensions.json), which will soon be "
                      "deprecated. (see http://developer.chrome.com/"
                      "extensions/external_extensions.html)";

    ReadStandaloneExtensionPrefFiles(prefs.get());
  }

  prefs_.swap(prefs);

  if (base_path_id_ == chrome::DIR_EXTERNAL_EXTENSIONS) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExternalJsonCount",
                             prefs_->size());
  }

  // If we have any records to process, then we must have
  // read at least one .json file.  If so, then we should have
  // set |base_path_|.
  if (!prefs_->empty())
    CHECK(!base_path_.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalPrefLoader::LoadFinished, this));
}

void ExternalPrefLoader::ReadExternalExtensionPrefFile(
    base::DictionaryValue* prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(NULL != prefs);

  base::FilePath json_file = base_path_.Append(kExternalExtensionJson);

  if (!base::PathExists(json_file)) {
    // This is not an error.  The file does not exist by default.
    return;
  }

  if (IsOptionSet(ENSURE_PATH_CONTROLLED_BY_ADMIN)) {
#if defined(OS_MACOSX)
    if (!base::VerifyPathControlledByAdmin(json_file)) {
      LOG(ERROR) << "Can not read external extensions source.  The file "
                 << json_file.value() << " and every directory in its path, "
                 << "must be owned by root, have group \"admin\", and not be "
                 << "writable by all users. These restrictions prevent "
                 << "unprivleged users from making chrome install extensions "
                 << "on other users' accounts.";
      return;
    }
#else
    // The only platform that uses this check is Mac OS.  If you add one,
    // you need to implement base::VerifyPathControlledByAdmin() for
    // that platform.
    NOTREACHED();
#endif  // defined(OS_MACOSX)
  }

  JSONFileValueDeserializer deserializer(json_file);
  scoped_ptr<base::DictionaryValue> ext_prefs(
      ExtractExtensionPrefs(&deserializer, json_file));
  if (ext_prefs)
    prefs->MergeDictionary(ext_prefs.get());
}

void ExternalPrefLoader::ReadStandaloneExtensionPrefFiles(
    base::DictionaryValue* prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CHECK(NULL != prefs);

  // First list the potential .json candidates.
  std::set<base::FilePath>
      candidates = GetPrefsCandidateFilesFromFolder(base_path_);
  if (candidates.empty()) {
    DVLOG(1) << "Extension candidates list empty";
    return;
  }

  // For each file read the json description & build the proper
  // associated prefs.
  for (std::set<base::FilePath>::const_iterator it = candidates.begin();
       it != candidates.end();
       ++it) {
    base::FilePath extension_candidate_path = base_path_.Append(*it);

    std::string id =
#if defined(OS_WIN)
        base::UTF16ToASCII(
            extension_candidate_path.RemoveExtension().BaseName().value());
#elif defined(OS_POSIX)
        extension_candidate_path.RemoveExtension().BaseName().value();
#endif

    DVLOG(1) << "Reading json file: "
             << extension_candidate_path.LossyDisplayName();

    JSONFileValueDeserializer deserializer(extension_candidate_path);
    scoped_ptr<base::DictionaryValue> ext_prefs(
        ExtractExtensionPrefs(&deserializer, extension_candidate_path));
    if (ext_prefs) {
      DVLOG(1) << "Adding extension with id: " << id;
      prefs->Set(id, ext_prefs.release());
    }
  }
}

ExternalTestingLoader::ExternalTestingLoader(
    const std::string& json_data,
    const base::FilePath& fake_base_path)
    : fake_base_path_(fake_base_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JSONStringValueDeserializer deserializer(json_data);
  base::FilePath fake_json_path = fake_base_path.AppendASCII("fake.json");
  testing_prefs_.reset(ExtractExtensionPrefs(&deserializer, fake_json_path));
}

void ExternalTestingLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_.reset(testing_prefs_->DeepCopy());
  LoadFinished();
}

ExternalTestingLoader::~ExternalTestingLoader() {}

const base::FilePath ExternalTestingLoader::GetBaseCrxFilePath() {
  return fake_base_path_;
}

}  // namespace extensions
