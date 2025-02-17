// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/capture/webm_muxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::WithArgs;

namespace media {

class WebmMuxerTest : public TestWithParam<VideoCodec> {
 public:
  WebmMuxerTest()
      : webm_muxer_(
            GetParam() /* codec */,
            base::Bind(&WebmMuxerTest::WriteCallback, base::Unretained(this))),
        last_encoded_length_(0),
        accumulated_position_(0) {
    EXPECT_EQ(webm_muxer_.Position(), 0);
    const mkvmuxer::int64 kRandomNewPosition = 333;
    EXPECT_EQ(webm_muxer_.Position(kRandomNewPosition), -1);
    EXPECT_FALSE(webm_muxer_.Seekable());
  }

  MOCK_METHOD1(WriteCallback, void(base::StringPiece));

  void SaveEncodedDataLen(const base::StringPiece& encoded_data) {
    last_encoded_length_ = encoded_data.size();
    accumulated_position_ += encoded_data.size();
  }

  mkvmuxer::int64 GetWebmMuxerPosition() const {
    return webm_muxer_.Position();
  }

  mkvmuxer::Segment::Mode GetWebmSegmentMode() const {
    return webm_muxer_.segment_.mode();
  }

  mkvmuxer::int32 WebmMuxerWrite(const void* buf, mkvmuxer::uint32 len) {
    return webm_muxer_.Write(buf, len);
  }

  WebmMuxer webm_muxer_;

  size_t last_encoded_length_;
  int64_t accumulated_position_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebmMuxerTest);
};

// Checks that the WriteCallback is called with appropriate params when
// WebmMuxer::Write() method is called.
TEST_P(WebmMuxerTest, Write) {
  const base::StringPiece encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(encoded_data));
  WebmMuxerWrite(encoded_data.data(), encoded_data.size());

  EXPECT_EQ(GetWebmMuxerPosition(), static_cast<int64_t>(encoded_data.size()));
}

// This test sends two frames and checks that the WriteCallback is called with
// appropriate params in both cases.
TEST_P(WebmMuxerTest, OnEncodedVideoTwoFrames) {
  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const std::string encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(WithArgs<0>(
          Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(video_frame,
                             make_scoped_ptr(new std::string(encoded_data)),
                             base::TimeTicks::Now(),
                             false  /* keyframe */);

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));
  EXPECT_EQ(GetWebmSegmentMode(), mkvmuxer::Segment::kLive);

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(WithArgs<0>(
          Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(video_frame,
                             make_scoped_ptr(new std::string(encoded_data)),
                             base::TimeTicks::Now(),
                             false  /* keyframe */);

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

INSTANTIATE_TEST_CASE_P(, WebmMuxerTest, Values(kCodecVP8, kCodecVP9));

}  // namespace media
