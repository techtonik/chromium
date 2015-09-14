// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/value_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using content::WebContents;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ListBuilder;
using extensions::TestExtensionDir;
using testing::ContainerEq;
using testing::ElementsAre;

namespace {

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails()
      : MetricsMemoryDetails(base::Bind(&base::DoNothing), nullptr) {}

  void StartFetchAndWait() {
    uma_.reset(new base::HistogramTester());
    StartFetch(FROM_CHROME_ONLY);
    content::RunMessageLoop();
  }

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

 private:
  ~TestMemoryDetails() override {}

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    base::MessageLoop::current()->Quit();
  }

  scoped_ptr<base::HistogramTester> uma_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryDetails);
};

}  // namespace

class SiteDetailsBrowserTest : public ExtensionBrowserTest {
 public:
  SiteDetailsBrowserTest() {}
  ~SiteDetailsBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  // Create and install an extension that has a couple of web-accessible
  // resources and, optionally, a background process.
  const Extension* CreateExtension(const std::string& name,
                                   bool has_background_process) {
    scoped_ptr<TestExtensionDir> dir(new TestExtensionDir);

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources", ListBuilder()
                                             .Append("blank_iframe.html")
                                             .Append("http_iframe.html"));

    if (has_background_process) {
      manifest.Set("background",
                   DictionaryBuilder().Set("scripts",
                                           ListBuilder().Append("script.js")));
      dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('" + name + " running');");
    }

    dir->WriteFile(FILE_PATH_LITERAL("blank_iframe.html"),
                   "<html><body>" + name +
                       ", blank iframe: "
                       "<iframe width=40 height=40></iframe></body></html>");
    GURL iframe_url = embedded_test_server()->GetURL("w.com", "/title1.html");
    dir->WriteFile(FILE_PATH_LITERAL("http_iframe.html"),
                   "<html><body>" + name +
                       ", http:// iframe: "
                       "<iframe width=40 height=40 src='" +
                       iframe_url.spec() + "'></iframe></body></html>");
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->unpacked_path());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(dir.release());
    return extension;
  }

 private:
  ScopedVector<TestExtensionDir> temp_dirs_;
  DISALLOW_COPY_AND_ASSIGN(SiteDetailsBrowserTest);
};

// Test the accuracy of SiteDetails process estimation, in the presence of
// multiple iframes, navigation, multiple BrowsingInstances, and multiple tabs
// in the same BrowsingInstance.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, ManyIframes) {
  // Page with 14 nested oopifs across 9 sites (a.com through i.com).
  // None of these are https.
  GURL abcdefghi_url = embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b(a(b,c,d,e,f,g,h)),c,d,e,i(f))");
  ui_test_utils::NavigateToURL(browser(), abcdefghi_url);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));

  // Navigate to a different, disjoint set of 7 sites.
  GURL pqrstuv_url = embedded_test_server()->GetURL(
      "p.com",
      "/cross_site_iframe_factory.html?p(q(r),r(s),s(t),t(q),u(u),v(p))");
  ui_test_utils::NavigateToURL(browser(), pqrstuv_url);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));

  // Open a second tab (different BrowsingInstance) with 4 sites (a through d).
  GURL abcd_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d())))");
  AddTabAtIndex(1, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 2.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));

  // Open a third tab (different BrowsingInstance) with the same 4 sites.
  AddTabAtIndex(2, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));

  // From the third tab, window.open() a fourth tab in the same
  // BrowsingInstance, to a page using the same four sites "a-d" as third tab,
  // plus an additional site "e". The estimated process counts should increase
  // by one (not five) from the previous scenario, as the new tab can reuse the
  // four processes already in the BrowsingInstance.
  GURL dcbae_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?d(c(b(a(e))))");
  ui_test_utils::UrlLoadObserver load_complete(
      dcbae_url, content::NotificationService::AllSources());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + dcbae_url.spec() + "');"));
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  load_complete.Wait();

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(12, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
}

IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, IsolateExtensions) {
  // We start on "about:blank", which should be credited with a process in this
  // case.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));

  // Install one script-injecting extension with background page, and an
  // extension with web accessible resources.
  const Extension* extension1 = CreateExtension("Extension One", true);
  const Extension* extension2 = CreateExtension("Extension Two", false);

  // Open two a.com tabs (with cross site http iframes). IsolateExtensions mode
  // should have no effect so far, since there are no frames straddling the
  // extension/web boundary.
  GURL tab1_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ui_test_utils::NavigateToURL(browser(), tab1_url);
  WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  GURL tab2_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(d,e)");
  AddTabAtIndex(1, tab2_url, ui::PAGE_TRANSITION_TYPED);
  WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(1);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));

  // Test that "one process per extension" applies even when web content has an
  // extension iframe.

  // Tab1 navigates its first iframe to a resource of extension1. This shouldn't
  // result in a new extension process (it should share with extension1's
  // background page).
  content::NavigateIframeToURL(
      tab1, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));

  // Tab2 navigates its first iframe to a resource of extension1. This also
  // shouldn't result in a new extension process (it should share with the
  // background page and the other iframe).
  content::NavigateIframeToURL(
      tab2, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));

  // Tab1 navigates its second iframe to a resource of extension2. This SHOULD
  // result in a new process since extension2 had no existing process.
  content::NavigateIframeToURL(
      tab1, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));

  // Tab2 navigates its second iframe to a resource of extension2. This should
  // share the existing extension2 process.
  content::NavigateIframeToURL(
      tab2, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));

  // Install extension3 (identical config to extension2)
  const Extension* extension3 = CreateExtension("Extension Three", false);

  // Navigate Tab2 to a top-level page from extension3. There are four processes
  // now: one for tab1's main frame, and one for each of the extensions:
  // extension1 has a process because it has a background page; extension2 is
  // used as an iframe in tab1, and extension3 is the top-level frame in tab2.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));

  // Navigate tab2 to a different extension3 page containing a web iframe. The
  // iframe should get its own process. The lower bound number indicates that,
  // in theory, the iframe could share a process with tab1's main frame.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(5, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(5, 1)));

  // Navigate tab1 to an extension3 page with an extension3 iframe. There should
  // be three processes estimated by IsolateExtensions: one for extension3, one
  // for extension1's background page, and one for the web iframe in tab2.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));

  // Now navigate tab1 to an extension3 page with a web iframe. This could share
  // a process with tab2's iframe (the LowerBound number), or it could get its
  // own process (the Estimate number).
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));
}
