// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::ElementsAre;

namespace blink {
namespace {

static PaintChunkProperties rootPaintChunkProperties() { return PaintChunkProperties(); }

class PaintChunkerTest : public testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

TEST_F(PaintChunkerTest, Empty)
{
    Vector<PaintChunk> chunks = PaintChunker().releasePaintChunks();
    ASSERT_TRUE(chunks.isEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 3, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    chunker.decrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();

    PaintChunkProperties simpleTransform;
    simpleTransform.transform = adoptRef(new TransformPaintPropertyNode(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7)));

    chunker.updateCurrentPaintChunkProperties(simpleTransform);
    chunker.incrementDisplayItemIndex();

    PaintChunkProperties anotherTransform;
    anotherTransform.transform = adoptRef(new TransformPaintPropertyNode(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7)));
    chunker.updateCurrentPaintChunkProperties(anotherTransform);
    chunker.incrementDisplayItemIndex();

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, rootPaintChunkProperties()),
        PaintChunk(2, 3, simpleTransform),
        PaintChunk(3, 4, anotherTransform)));
}

TEST_F(PaintChunkerTest, BuildLinearChunksFromNestedTransforms)
{
    // Test that "nested" transforms linearize using the following
    // sequence of transforms and display items:
    // <root xform>, <paint>, <a xform>, <paint>, <paint>, </a xform>, <paint>, </root xform>
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();

    PaintChunkProperties simpleTransform;
    simpleTransform.transform = adoptRef(new TransformPaintPropertyNode(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7)));
    chunker.updateCurrentPaintChunkProperties(simpleTransform);
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();

    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();

    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 3, simpleTransform),
        PaintChunk(3, 4, rootPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems)
{
    // Test that properties can change without display items being generated.
    PaintChunker chunker;
    chunker.updateCurrentPaintChunkProperties(rootPaintChunkProperties());
    chunker.incrementDisplayItemIndex();

    PaintChunkProperties firstTransform;
    firstTransform.transform = adoptRef(new TransformPaintPropertyNode(TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7)));
    chunker.updateCurrentPaintChunkProperties(firstTransform);

    PaintChunkProperties secondTransform;
    secondTransform.transform = adoptRef(new TransformPaintPropertyNode(TransformationMatrix(9, 8, 7, 6, 5, 4), FloatPoint3D(3, 2, 1)));
    chunker.updateCurrentPaintChunkProperties(secondTransform);

    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 1, rootPaintChunkProperties()),
        PaintChunk(1, 2, secondTransform)));
}

// TODO(pdr): Add more tests once we have more paint properties.

} // namespace
} // namespace blink
