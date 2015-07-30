// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_encoder.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/speech/audio_buffer.h"
#include "third_party/flac/include/FLAC/stream_encoder.h"

namespace content {
namespace {

//-------------------------------- FLACEncoder ---------------------------------

const char* const kContentTypeFLAC = "audio/x-flac; rate=";
const int kFLACCompressionLevel = 0;  // 0 for speed

class FLACEncoder : public AudioEncoder {
 public:
  FLACEncoder(int sampling_rate, int bits_per_sample);
  ~FLACEncoder() override;
  void Encode(const AudioChunk& raw_audio) override;
  void Flush() override;

 private:
  static FLAC__StreamEncoderWriteStatus WriteCallback(
      const FLAC__StreamEncoder* encoder,
      const FLAC__byte buffer[],
      size_t bytes,
      unsigned samples,
      unsigned current_frame,
      void* client_data);

  FLAC__StreamEncoder* encoder_;
  bool is_encoder_initialized_;

  DISALLOW_COPY_AND_ASSIGN(FLACEncoder);
};

FLAC__StreamEncoderWriteStatus FLACEncoder::WriteCallback(
    const FLAC__StreamEncoder* encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void* client_data) {
  FLACEncoder* me = static_cast<FLACEncoder*>(client_data);
  DCHECK(me->encoder_ == encoder);
  me->encoded_audio_buffer_.Enqueue(buffer, bytes);
  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLACEncoder::FLACEncoder(int sampling_rate, int bits_per_sample)
    : AudioEncoder(std::string(kContentTypeFLAC) +
                   base::IntToString(sampling_rate),
                   bits_per_sample),
      encoder_(FLAC__stream_encoder_new()),
      is_encoder_initialized_(false) {
  FLAC__stream_encoder_set_channels(encoder_, 1);
  FLAC__stream_encoder_set_bits_per_sample(encoder_, bits_per_sample);
  FLAC__stream_encoder_set_sample_rate(encoder_, sampling_rate);
  FLAC__stream_encoder_set_compression_level(encoder_, kFLACCompressionLevel);

  // Initializing the encoder will cause sync bytes to be written to
  // its output stream, so we wait until the first call to this method
  // before doing so.
}

FLACEncoder::~FLACEncoder() {
  FLAC__stream_encoder_delete(encoder_);
}

void FLACEncoder::Encode(const AudioChunk& raw_audio) {
  DCHECK_EQ(raw_audio.bytes_per_sample(), 2);
  if (!is_encoder_initialized_) {
    const FLAC__StreamEncoderInitStatus encoder_status =
        FLAC__stream_encoder_init_stream(encoder_, WriteCallback, NULL, NULL,
                                         NULL, this);
    DCHECK_EQ(encoder_status, FLAC__STREAM_ENCODER_INIT_STATUS_OK);
    is_encoder_initialized_ = true;
  }

  // FLAC encoder wants samples as int32s.
  const int num_samples = raw_audio.NumSamples();
  scoped_ptr<FLAC__int32[]> flac_samples(new FLAC__int32[num_samples]);
  FLAC__int32* flac_samples_ptr = flac_samples.get();
  for (int i = 0; i < num_samples; ++i)
    flac_samples_ptr[i] = static_cast<FLAC__int32>(raw_audio.GetSample16(i));

  FLAC__stream_encoder_process(encoder_, &flac_samples_ptr, num_samples);
}

void FLACEncoder::Flush() {
  FLAC__stream_encoder_finish(encoder_);
}

}  // namespace

AudioEncoder* AudioEncoder::Create(int sampling_rate, int bits_per_sample) {
  return new FLACEncoder(sampling_rate, bits_per_sample);
}

AudioEncoder::AudioEncoder(const std::string& mime_type, int bits_per_sample)
    : encoded_audio_buffer_(1), /* Byte granularity of encoded samples. */
      mime_type_(mime_type),
      bits_per_sample_(bits_per_sample) {
}

AudioEncoder::~AudioEncoder() {
}

scoped_refptr<AudioChunk> AudioEncoder::GetEncodedDataAndClear() {
  return encoded_audio_buffer_.DequeueAll();
}

}  // namespace content
