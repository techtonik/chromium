// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandler.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"

namespace blink {
class WebMediaRecorderHandlerClient;
class WebString;
}  // namespace blink

namespace media {
class VideoFrame;
class WebmMuxer;
}  // namespace media

namespace content {

class VideoTrackRecorder;

// MediaRecorderHandler orchestrates the creation, lifetime management and
// mapping between:
// - MediaStreamTrack(s) providing data,
// - {Audio,Video}TrackRecorders encoding that data,
// - a WebmMuxer class multiplexing encoded data into a WebM container, and
// - a single recorder client receiving this contained data.
// All methods are called on the same thread as construction and destruction,
// i.e. the Main Render thread. (Note that a BindToCurrentLoop is used to
// guarantee this, since VideoTrackRecorder sends back frames on IO thread.)
// TODO(mcasas): http://crbug.com/528519 Implement audio recording.
class CONTENT_EXPORT MediaRecorderHandler final
    : public NON_EXPORTED_BASE(blink::WebMediaRecorderHandler) {
 public:
  MediaRecorderHandler();
  ~MediaRecorderHandler() override;

  // blink::WebMediaRecorderHandler.
  bool canSupportMimeType(const blink::WebString& mimeType) override;
  bool initialize(blink::WebMediaRecorderHandlerClient* client,
                  const blink::WebMediaStream& media_stream,
                  const blink::WebString& mimeType) override;
  bool start() override;
  bool start(int timeslice) override;
  void stop() override;
  void pause() override;
  void resume() override;

 private:
  friend class MediaRecorderHandlerTest;

  void OnEncodedVideo(const scoped_refptr<media::VideoFrame>& video_frame,
                      scoped_ptr<std::string> encoded_data,
                      base::TimeTicks timestamp,
                      bool is_key_frame);
  void WriteData(base::StringPiece data);

  void OnVideoFrameForTesting(const scoped_refptr<media::VideoFrame>& frame,
                              const base::TimeTicks& timestamp);

  // Bound to the main render thread.
  base::ThreadChecker main_render_thread_checker_;

  // Force using VP9 for video encoding, otherwise VP8 will be used by default.
  bool use_vp9_;

  bool recording_;
  blink::WebMediaStream media_stream_;  // The MediaStream being recorded.

  // |client_| is a weak pointer, and is valid for the lifetime of this object.
  blink::WebMediaRecorderHandlerClient* client_;

  ScopedVector<VideoTrackRecorder> video_recorders_;

  // Worker class doing the actual Webm Muxing work.
  scoped_ptr<media::WebmMuxer> webm_muxer_;

  base::WeakPtrFactory<MediaRecorderHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRecorderHandler);
};

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_
