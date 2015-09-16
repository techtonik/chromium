// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "components/sessions/session_id.h"

namespace base {
class CancelableTaskTracker;
class SequencedWorkerPool;
}

// TODO(blundell): Remove all references to WebContents from
// TabRestoreServiceClient and get rid of this. crbug.com/530174
namespace content {
class WebContents;
}

namespace sessions {
struct SessionWindow;
}

class GURL;
class TabRestoreServiceDelegate;

namespace sessions {

struct SessionWindow;

// Callback from TabRestoreServiceClient::GetLastSession.
// The second parameter is the id of the window that was last active.
typedef base::Callback<void(ScopedVector<SessionWindow>, SessionID::id_type)>
    GetLastSessionCallback;

// A client interface that needs to be supplied to the tab restore service by
// the embedder.
class TabRestoreServiceClient {
 public:
  virtual ~TabRestoreServiceClient() {}

  // Creates a TabRestoreServiceDelegate instance that is associated with
  // |host_desktop_type| and |app_name|. May return nullptr (e.g., if the
  // embedder does not support TabRestoreServiceDelegate functionality).
  // Note that |host_desktop_type| is opaque to the component; the only values
  // that will be passed here are those that have been passed *in* to the
  // component from the embedder via TabRestoreService.
  virtual TabRestoreServiceDelegate* CreateTabRestoreServiceDelegate(
      int host_desktop_type,
      const std::string& app_name) = 0;

  // Returns the TabRestoreServiceDelegate instance that is associated with
  // |contents|, or null if there is no such instance.
  // TODO(blundell): Replace the usage of WebContents here with the cross-
  // platform interface that will abstract it. crbug.com/530174
  virtual TabRestoreServiceDelegate*
  FindTabRestoreServiceDelegateForWebContents(
      const content::WebContents* contents) = 0;

  // Returns the TabRestoreServiceDelegate instance that is associated with
  // |desired_id| and |host_desktop_type|, or null if there is no such instance.
  // Note that |host_desktop_type| is opaque to the component; the only values
  // that will be passed here are those that have been passed *in* to the
  // component from the embedder via TabRestoreService.
  virtual TabRestoreServiceDelegate* FindTabRestoreServiceDelegateWithID(
      SessionID::id_type desired_id,
      int host_desktop_type) = 0;

  // Returns whether a given URL should be tracked for restoring.
  virtual bool ShouldTrackURLForRestore(const GURL& url) = 0;

  // Returns the extension app ID for the given WebContents, or the empty string
  // if there is no such ID (e.g., if extensions are not supported by the
  // embedder).
  // TODO(blundell): Replace the usage of WebContents here with the cross-
  // platform interface that will abstract it. crbug.com/530174
  virtual std::string GetExtensionAppIDForWebContents(
      content::WebContents* web_contents) = 0;

  // Get the sequenced worker pool for running tasks on the backend thread as
  // long as the system is not shutting down.
  virtual base::SequencedWorkerPool* GetBlockingPool() = 0;

  // Returns the path of the directory to save state into.
  virtual base::FilePath GetPathToSaveTo() = 0;

  // Returns the URL that corresponds to the new tab page.
  virtual GURL GetNewTabURL() = 0;

  // Returns whether there is a previous session to load.
  virtual bool HasLastSession() = 0;

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  virtual void GetLastSession(const GetLastSessionCallback& callback,
                              base::CancelableTaskTracker* tracker) = 0;

  // Called when a tab is restored. |url| is the URL that the tab is currently
  // visiting.
  virtual void OnTabRestored(const GURL& url) {}
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
