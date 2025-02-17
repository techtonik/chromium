// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/gl/gl_switches.h"

using gpu::GpuFeatureType;

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

class WebstoreInstallListener : public WebstoreInstaller::Delegate {
 public:
  WebstoreInstallListener()
      : received_failure_(false), received_success_(false), waiting_(false) {}

  void OnExtensionInstallSuccess(const std::string& id) override {
    received_success_ = true;
    id_ = id;

    if (waiting_) {
      waiting_ = false;
      base::MessageLoopForUI::current()->QuitWhenIdle();
    }
  }

  void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) override {
    received_failure_ = true;
    id_ = id;
    error_ = error;

    if (waiting_) {
      waiting_ = false;
      base::MessageLoopForUI::current()->QuitWhenIdle();
    }
  }

  void Wait() {
    if (received_success_ || received_failure_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }
  bool received_success() const { return received_success_; }
  const std::string& id() const { return id_; }

 private:
  bool received_failure_;
  bool received_success_;
  bool waiting_;
  std::string id_;
  std::string error_;
};

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public ExtensionApiTest {
 public:
  ExtensionWebstorePrivateApiTest() {}
  ~ExtensionWebstorePrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL,
        "http://www.example.com/files/extensions/api_test");
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(StartSpawnedTestServer());
    extensions::ExtensionInstallUI::set_disable_failure_ui_for_tests();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    auto_confirm_install_.reset(
        new ScopedTestDialogAutoConfirm(ScopedTestDialogAutoConfirm::ACCEPT));

    ASSERT_TRUE(webstore_install_dir_.CreateUniqueTempDir());
    webstore_install_dir_copy_ = webstore_install_dir_.path();
    WebstoreInstaller::SetDownloadDirectoryForTests(
        &webstore_install_dir_copy_);
  }

 protected:
  // Returns a test server URL, but with host 'www.example.com' so it matches
  // the web store app's extent that we set up via command line flags.
  GURL DoGetTestServerURL(const std::string& path) {
    GURL url = test_server()->GetURL(path);

    // Replace the host with 'www.example.com' so it matches the web store
    // app's extent.
    GURL::Replacements replace_host;
    replace_host.SetHostStr("www.example.com");

    return url.ReplaceComponents(replace_host);
  }

  virtual GURL GetTestServerURL(const std::string& path) {
    return DoGetTestServerURL(
        std::string("files/extensions/api_test/webstore_private/") + path);
  }

  // Navigates to |page| and runs the Extension API test there. Any downloads
  // of extensions will return the contents of |crx_file|.
  bool RunInstallTest(const std::string& page, const std::string& crx_file) {
#if defined(OS_WIN) && !defined(NDEBUG)
    // See http://crbug.com/177163 for details.
    return true;
#else
    GURL crx_url = GetTestServerURL(crx_file);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());

    GURL page_url = GetTestServerURL(page);
    return RunPageTest(page_url.spec());
#endif
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ExtensionService* service() {
    return ExtensionSystem::Get(browser()->profile())->extension_service();
  }

 private:
  base::ScopedTempDir webstore_install_dir_;
  // WebstoreInstaller needs a reference to a FilePath when setting the download
  // directory for testing.
  base::FilePath webstore_install_dir_copy_;

  scoped_ptr<ScopedTestDialogAutoConfirm> auto_confirm_install_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebstorePrivateApiTest);
};

// Test cases for webstore origin frame blocking.
// TODO(mkwst): Disabled until new X-Frame-Options behavior rolls into
// Chromium, see crbug.com/226018.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       DISABLED_FrameWebstorePageBlocked) {
  base::string16 expected_title = base::UTF8ToUTF16("PASS: about:blank");
  base::string16 failure_title = base::UTF8ToUTF16("FAIL");
  content::TitleWatcher watcher(GetWebContents(), expected_title);
  watcher.AlsoWaitForTitle(failure_title);
  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webstore_private/noframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::string16 final_title = watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

// TODO(mkwst): Disabled until new X-Frame-Options behavior rolls into
// Chromium, see crbug.com/226018.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       DISABLED_FrameErrorPageBlocked) {
  base::string16 expected_title = base::UTF8ToUTF16("PASS: about:blank");
  base::string16 failure_title = base::UTF8ToUTF16("FAIL");
  content::TitleWatcher watcher(GetWebContents(), expected_title);
  watcher.AlsoWaitForTitle(failure_title);
  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webstore_private/noframe2.html");
  ui_test_utils::NavigateToURL(browser(), url);
  base::string16 final_title = watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

// Test cases where the user accepts the install confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallAccepted) {
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));
}

// Test having the default download directory missing.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MissingDownloadDir) {
  // Set a non-existent directory as the download path.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath missing_directory = temp_dir.Take();
  EXPECT_TRUE(base::DeleteFile(missing_directory, true));
  WebstoreInstaller::SetDownloadDirectoryForTests(&missing_directory);

  // Now run the install test, which should succeed.
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));

  // Cleanup.
  if (base::DirectoryExists(missing_directory))
    EXPECT_TRUE(base::DeleteFile(missing_directory, true));
}

// Tests passing a localized name.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallLocalized) {
  ASSERT_TRUE(RunInstallTest("localized.html", "localized_extension.crx"));
}

// Now test the case where the user cancels the confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallCancelled) {
  ScopedTestDialogAutoConfirm auto_cancel(ScopedTestDialogAutoConfirm::CANCEL);
  ASSERT_TRUE(RunInstallTest("cancelled.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest1) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest1.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest2) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest2.html", "extension.crx"));
}

// Disabled: http://crbug.com/174399 and http://crbug.com/177163
#if defined(OS_WIN) && (defined(USE_AURA) || !defined(NDEBUG))
#define MAYBE_AppInstallBubble DISABLED_AppInstallBubble
#else
#define MAYBE_AppInstallBubble AppInstallBubble
#endif

// Tests that we can request an app installed bubble (instead of the default
// UI when an app is installed).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest,
                       MAYBE_AppInstallBubble) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("app_install_bubble.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iladmdjkfniedhfhcfoefgojhgaiaccc", listener.id());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IsInIncognitoMode) {
  GURL page_url = GetTestServerURL("incognito.html");
  ASSERT_TRUE(
      RunPageTest(page_url.spec(), ExtensionApiTest::kFlagUseIncognito));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IsNotInIncognitoMode) {
  GURL page_url = GetTestServerURL("not_incognito.html");
  ASSERT_TRUE(RunPageTest(page_url.spec()));
}

// Fails often on Windows dbg bots. http://crbug.com/177163.
#if defined(OS_WIN)
#define MAYBE_IconUrl DISABLED_IconUrl
#else
#define MAYBE_IconUrl IconUrl
#endif  // defined(OS_WIN)
// Tests using the iconUrl parameter to the install function.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MAYBE_IconUrl) {
  ASSERT_TRUE(RunInstallTest("icon_url.html", "extension.crx"));
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_BeginInstall DISABLED_BeginInstall
#else
#define MAYBE_BeginInstall BeginInstall
#endif
// Tests that the Approvals are properly created in beginInstall.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MAYBE_BeginInstall) {
  std::string appId = "iladmdjkfniedhfhcfoefgojhgaiaccc";
  std::string extensionId = "enfkhcelefdadlmkffamgdlgplcionje";
  ASSERT_TRUE(RunInstallTest("begin_install.html", "extension.crx"));

  scoped_ptr<WebstoreInstaller::Approval> approval =
      WebstorePrivateApi::PopApprovalForTesting(browser()->profile(), appId);
  EXPECT_EQ(appId, approval->extension_id);
  EXPECT_TRUE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_EQ("2", approval->authuser);
  EXPECT_EQ(browser()->profile(), approval->profile);

  approval = WebstorePrivateApi::PopApprovalForTesting(
      browser()->profile(), extensionId);
  EXPECT_EQ(extensionId, approval->extension_id);
  EXPECT_FALSE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_TRUE(approval->authuser.empty());
  EXPECT_EQ(browser()->profile(), approval->profile);
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InstallTheme DISABLED_InstallTheme
#else
#define MAYBE_InstallTheme InstallTheme
#endif
// Tests that themes are installed without an install prompt.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MAYBE_InstallTheme) {
  WebstoreInstallListener listener;
  WebstorePrivateApi::SetWebstoreInstallerDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("theme.html", "../../theme.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iamefpfkojoapidjnbafmgkgncegbkad", listener.id());
}

// Tests that an error is properly reported when an empty crx is returned.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, EmptyCrx) {
  ASSERT_TRUE(RunInstallTest("empty.html", "empty.crx"));
}

class ExtensionWebstoreGetWebGLStatusTest : public InProcessBrowserTest {
 protected:
  void RunTest(bool webgl_allowed) {
    // If Gpu access is disallowed then WebGL will not be available.
    if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(NULL))
      webgl_allowed = false;

    static const char kEmptyArgs[] = "[]";
    static const char kWebGLStatusAllowed[] = "webgl_allowed";
    static const char kWebGLStatusBlocked[] = "webgl_blocked";
    scoped_refptr<WebstorePrivateGetWebGLStatusFunction> function =
        new WebstorePrivateGetWebGLStatusFunction();
    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
            function.get(), kEmptyArgs, browser()));
    ASSERT_TRUE(result);
    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string webgl_status;
    EXPECT_TRUE(result->GetAsString(&webgl_status));
    EXPECT_STREQ(webgl_allowed ? kWebGLStatusAllowed : kWebGLStatusBlocked,
                 webgl_status.c_str());
  }
};

// Tests getWebGLStatus function when WebGL is allowed.
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, Allowed) {
  bool webgl_allowed = true;
  RunTest(webgl_allowed);
}

// Tests getWebGLStatus function when WebGL is blacklisted.
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, Blocked) {
  static const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"features\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  gpu::GPUInfo gpu_info;
  content::GpuDataManager::GetInstance()->InitializeForTesting(
      json_blacklist, gpu_info);
  EXPECT_TRUE(content::GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_WEBGL));

  bool webgl_allowed = false;
  RunTest(webgl_allowed);
}

class EphemeralAppWebstorePrivateApiTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionWebstorePrivateApiTest::SetUpInProcessBrowserTestFixture();

    net::HostPortPair host_port = test_server()->host_port_pair();
    std::string test_gallery_url = base::StringPrintf(
        "http://www.example.com:%d/files/extensions/platform_apps/"
        "ephemeral_launcher",
        host_port.port());
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryURL, test_gallery_url);
  }

  GURL GetTestServerURL(const std::string& path) override {
    return DoGetTestServerURL(
        std::string("files/extensions/platform_apps/ephemeral_launcher/") +
        path);
  }
};

// Run tests when the --enable-ephemeral-apps switch is not enabled.
IN_PROC_BROWSER_TEST_F(EphemeralAppWebstorePrivateApiTest,
                       EphemeralAppsFeatureDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      app_list::switches::kDisableExperimentalAppList);
  ASSERT_TRUE(RunInstallTest("webstore_launch_disabled.html", "app.crx"));
}

// Run tests when the --enable-ephemeral-apps switch is enabled.
IN_PROC_BROWSER_TEST_F(EphemeralAppWebstorePrivateApiTest, LaunchEphemeralApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableEphemeralAppsInWebstore);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      app_list::switches::kEnableExperimentalAppList);
  ASSERT_TRUE(RunInstallTest("webstore_launch_app.html", "app.crx"));
}

class BundleWebstorePrivateApiTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionWebstorePrivateApiTest::SetUpInProcessBrowserTestFixture();

    test_data_dir_ = test_data_dir_.AppendASCII("webstore_private/bundle");

    // The test server needs to have already started, so setup the switch here
    // rather than in SetUpCommandLine.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryDownloadURL,
        GetTestServerURL("bundle/%s.crx").spec());
  }
};

// Tests successfully installing a bundle of 2 apps and 2 extensions.
IN_PROC_BROWSER_TEST_F(BundleWebstorePrivateApiTest, InstallBundle) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);
  ASSERT_TRUE(RunPageTest(GetTestServerURL("install_bundle.html").spec()));
}

// Tests that bundles can be installed from incognito windows.
IN_PROC_BROWSER_TEST_F(BundleWebstorePrivateApiTest, InstallBundleIncognito) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);

  ASSERT_TRUE(RunPageTest(GetTestServerURL("install_bundle.html").spec(),
                          ExtensionApiTest::kFlagUseIncognito));
}

// Tests the user canceling the bundle install prompt.
IN_PROC_BROWSER_TEST_F(BundleWebstorePrivateApiTest, InstallBundleCancel) {
  // We don't need to create the CRX files since we are aborting the install.
  extensions::BundleInstaller::SetAutoApproveForTesting(false);

  ASSERT_TRUE(
      RunPageTest(GetTestServerURL("install_bundle_cancel.html").spec()));
}

// Tests partially installing a bundle (1 succeeds, 1 fails due to an invalid
// CRX, 1 fails due to the manifests not matching, and 1 fails due to a missing
// crx file).
IN_PROC_BROWSER_TEST_F(BundleWebstorePrivateApiTest, InstallBundleInvalid) {
  extensions::BundleInstaller::SetAutoApproveForTesting(true);

  ASSERT_TRUE(
      RunPageTest(GetTestServerURL("install_bundle_invalid.html").spec()));
}

}  // namespace extensions
