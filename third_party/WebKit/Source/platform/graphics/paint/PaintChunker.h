// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunker_h
#define PaintChunker_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "wtf/Vector.h"

namespace blink {

// Accepts information about changes to |PaintChunkProperties| as drawings are
// accumulated, and produces a series of paint chunks: contiguous ranges of the
// display list with identical |PaintChunkProperties|.
class PLATFORM_EXPORT PaintChunker {
public:
    PaintChunker();
    ~PaintChunker();

    bool isInInitialState() const { return m_chunks.isEmpty() && m_currentProperties == PaintChunkProperties(); }

    void updateCurrentPaintChunkProperties(const PaintChunkProperties&);

    void incrementDisplayItemIndex();
    void decrementDisplayItemIndex();

    // Releases the generated paint chunk list and resets the state of this
    // object.
    Vector<PaintChunk> releasePaintChunks();

private:
    Vector<PaintChunk> m_chunks;
    PaintChunkProperties m_currentProperties;
};

} // namespace blink

#endif // PaintChunker_h
