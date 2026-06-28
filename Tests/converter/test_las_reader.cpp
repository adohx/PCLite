#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>

#include "attribute_reader/attribute_reader.h"
#include "attribute_reader/las_reader.h"

namespace {
std::string officeLasPath() {
    return std::string(TEST_DATA_DIR) + "/office.las";
}
}

TEST(LasReader, HeaderInfoMatchesOfficeLas) {
    auto reader = std::make_shared<LasReader>(officeLasPath());

    EXPECT_EQ(reader->getType(), AttributeReader::LAS);

    auto info = reader->headerInfo();
    EXPECT_EQ(info.extendedNumPoints_, 49092975u);

    EXPECT_NEAR(info.min_.x, -16.827, 1e-6);
    EXPECT_NEAR(info.min_.y, -25.671, 1e-6);
    EXPECT_NEAR(info.min_.z, -1.457, 1e-6);
    EXPECT_NEAR(info.max_.x, 21.557, 1e-6);
    EXPECT_NEAR(info.max_.y, 43.637, 1e-6);
    EXPECT_NEAR(info.max_.z, 4.597, 1e-6);
}

TEST(LasReader, AttributesMatchPointFormat2Layout) {
    auto reader = std::make_shared<LasReader>(officeLasPath());
    auto &attrs = reader->getAttributes();

    EXPECT_EQ(attrs.getTotalBytes(), 27u);

    auto position = attrs.getAttribute("position");
    EXPECT_EQ(position.numElements_, 3);
    EXPECT_EQ(position.bytes_, 12);
    EXPECT_EQ(position.type_, AttributeType::INT32);
    EXPECT_NEAR(position.scale_.x, 0.001, 1e-9);
    EXPECT_NEAR(position.offset_.x, 0.0, 1e-9);
    EXPECT_NEAR(position.min_.x, -16.827, 1e-6);
    EXPECT_NEAR(position.max_.x, 21.557, 1e-6);

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
    auto reader = std::make_shared<LasReader>(officeLasPath());
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
    auto reader = std::make_shared<LasReader>(officeLasPath());
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
    auto reader = AttributeReader::createReader(officeLasPath());
    ASSERT_NE(reader, nullptr);
    EXPECT_EQ(reader->getType(), AttributeReader::LAS);
}

TEST(AttributeReaderFactory, ReturnsNullForUnsupportedExtension) {
    auto reader = AttributeReader::createReader("/tmp/does_not_exist.pcd");
    EXPECT_EQ(reader, nullptr);
}
