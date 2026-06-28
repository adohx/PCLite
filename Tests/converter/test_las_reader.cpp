#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>

#include "attribute_reader/attribute_reader.h"
#include "attribute_reader/las_reader.h"

namespace {
// Small (1000-point), fully deterministic fixture committed to git -- see
// tools/generate_sample_las.py. A 10x10x10 grid at integer coordinates
// 0..9 per axis, so bounding box/point count are exact, not approximate.
std::string sampleLasPath() {
    return std::string(TEST_DATA_DIR) + "/sample.las";
}
}

TEST(LasReader, HeaderInfoMatchesSampleLas) {
    auto reader = std::make_shared<LasReader>(sampleLasPath());

    EXPECT_EQ(reader->getType(), AttributeReader::LAS);

    auto info = reader->headerInfo();
    // sample.las is plain LAS 1.2 (no LAS 1.4 extended point count VLR), so
    // the point count lives in the legacy field; extendedNumPoints_ stays 0.
    EXPECT_EQ(info.numPoints_, 1000u);

    EXPECT_NEAR(info.min_.x, 0.0, 1e-6);
    EXPECT_NEAR(info.min_.y, 0.0, 1e-6);
    EXPECT_NEAR(info.min_.z, 0.0, 1e-6);
    EXPECT_NEAR(info.max_.x, 9.0, 1e-6);
    EXPECT_NEAR(info.max_.y, 9.0, 1e-6);
    EXPECT_NEAR(info.max_.z, 9.0, 1e-6);
}

TEST(LasReader, AttributesMatchPointFormat2Layout) {
    auto reader = std::make_shared<LasReader>(sampleLasPath());
    auto &attrs = reader->getAttributes();

    EXPECT_EQ(attrs.getTotalBytes(), 27u);

    auto position = attrs.getAttribute("position");
    EXPECT_EQ(position.numElements_, 3);
    EXPECT_EQ(position.bytes_, 12);
    EXPECT_EQ(position.type_, AttributeType::INT32);
    EXPECT_NEAR(position.scale_.x, 0.001, 1e-9);
    EXPECT_NEAR(position.offset_.x, 0.0, 1e-9);
    EXPECT_NEAR(position.min_.x, 0.0, 1e-6);
    EXPECT_NEAR(position.max_.x, 9.0, 1e-6);

    auto rgb = attrs.getAttribute("rgb");
    EXPECT_EQ(rgb.numElements_, 3);
    EXPECT_EQ(rgb.bytes_, 6);

    EXPECT_FALSE(attrs.getAttribute("intensity").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("return number").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("number of returns").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("classification").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("scan angle rank").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("user data").name_.empty());
    EXPECT_FALSE(attrs.getAttribute("point source id").name_.empty());
}

TEST(LasReader, ReadPositionWithinBoundingBox) {
    auto reader = std::make_shared<LasReader>(sampleLasPath());
    auto info = reader->headerInfo();

    auto p0 = reader->readPosition(0);
    EXPECT_GE(p0.x, info.min_.x - 1e-3);
    EXPECT_LE(p0.x, info.max_.x + 1e-3);
    EXPECT_GE(p0.y, info.min_.y - 1e-3);
    EXPECT_LE(p0.y, info.max_.y + 1e-3);
    EXPECT_GE(p0.z, info.min_.z - 1e-3);
    EXPECT_LE(p0.z, info.max_.z + 1e-3);

    auto positions = reader->readPositions(0, 10);
    ASSERT_EQ(positions.size(), 10u);
    EXPECT_DOUBLE_EQ(positions[0].x, p0.x);
    EXPECT_DOUBLE_EQ(positions[0].y, p0.y);
    EXPECT_DOUBLE_EQ(positions[0].z, p0.z);
}

TEST(LasReader, ReadRawDataDecodesToSamePosition) {
    auto reader = std::make_shared<LasReader>(sampleLasPath());
    auto &attrs = reader->getAttributes();
    auto position = attrs.getAttribute("position");
    auto rowBytes = attrs.getTotalBytes();

    auto raw = reader->readRawData(0, 4);
    ASSERT_EQ(raw.size(), rowBytes * 4);

    auto expected = reader->readPositions(0, 4);
    for (int i = 0; i < 4; ++i) {
        const uint8_t *row = raw.data() + i * rowBytes;
        int32_t x, y, z;
        std::memcpy(&x, row + 0, 4);
        std::memcpy(&y, row + 4, 4);
        std::memcpy(&z, row + 8, 4);

        double wx = x * position.scale_.x + position.offset_.x;
        double wy = y * position.scale_.y + position.offset_.y;
        double wz = z * position.scale_.z + position.offset_.z;

        EXPECT_DOUBLE_EQ(wx, expected[i].x);
        EXPECT_DOUBLE_EQ(wy, expected[i].y);
        EXPECT_DOUBLE_EQ(wz, expected[i].z);
    }
}

TEST(AttributeReaderFactory, CreatesLasReaderForLasExtension) {
    auto reader = AttributeReader::createReader(sampleLasPath());
    ASSERT_NE(reader, nullptr);
    EXPECT_EQ(reader->getType(), AttributeReader::LAS);
}

TEST(AttributeReaderFactory, ReturnsNullForUnsupportedExtension) {
    auto reader = AttributeReader::createReader("/tmp/does_not_exist.pcd");
    EXPECT_EQ(reader, nullptr);
}
