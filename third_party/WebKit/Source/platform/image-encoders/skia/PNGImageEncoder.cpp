/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"

#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
extern "C" {
#include "png.h"
}

namespace blink {

static void writeOutput(png_structp png, png_bytep data, png_size_t size)
{
    static_cast<Vector<unsigned char>*>(png_get_io_ptr(png))->append(data, size);
}

static bool encodePixels(IntSize imageSize, const unsigned char* inputPixels, Vector<unsigned char>* output)
{
    if (imageSize.width() <= 0 || imageSize.height() <= 0)
        return false;

    png_struct* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_info* info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(png ? &png : 0, info ? &info : 0);
        return false;
    }

    // Optimize compression for speed.
    // The parameters are the same as what libpng uses by default for RGB and RGBA images, except:
    // - the zlib compression level is 3 instead of 6, to avoid the lazy Ziv-Lempel match searching;
    // - the delta filter is 1 ("sub") instead of 5 ("all"), to reduce the filter computations.
    // The zlib memory level (8) and strategy (Z_FILTERED) will be set inside libpng.
    //
    // Avoid the zlib strategies Z_HUFFMAN_ONLY or Z_RLE.
    // Although they are the fastest for poorly-compressible images (e.g. photographs),
    // they are very slow for highly-compressible images (e.g. text, drawings or business graphics).
    png_set_compression_level(png, 3);
    png_set_filter(png, PNG_FILTER_TYPE_BASE, PNG_FILTER_SUB);

    png_set_write_fn(png, output, writeOutput, 0);
    png_set_IHDR(png, info, imageSize.width(), imageSize.height(),
                 8, PNG_COLOR_TYPE_RGB_ALPHA, 0, 0, 0);
    png_write_info(png, info);

    unsigned char* pixels = const_cast<unsigned char*>(inputPixels);
    const size_t pixelRowStride = imageSize.width() * 4;
    for (int y = 0; y < imageSize.height(); ++y) {
        png_write_row(png, pixels);
        pixels += pixelRowStride;
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    return true;
}

bool PNGImageEncoder::encode(const ImageDataBuffer& imageData, Vector<unsigned char>* output)
{
    if (!imageData.pixels())
        return false;

    return encodePixels(IntSize(imageData.width(), imageData.height()), imageData.pixels(), output);
}

} // namespace blink
