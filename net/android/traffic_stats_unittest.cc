// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/traffic_stats.h"

#include "base/run_loop.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

TEST(TrafficStatsAndroidTest, BasicsTest) {
  test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  int64_t bytes_before_request = -1;
  EXPECT_TRUE(android::traffic_stats::GetTotalTxBytes(&bytes_before_request));
  EXPECT_GE(bytes_before_request, 0);

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));
  request->Start();
  base::RunLoop().Run();

  // Bytes should increase because of the network traffic.
  int64_t bytes_after_request;
  EXPECT_TRUE(android::traffic_stats::GetTotalTxBytes(&bytes_after_request));
  DCHECK_GT(bytes_after_request, bytes_before_request);
}

}  // namespace

}  // namespace net
