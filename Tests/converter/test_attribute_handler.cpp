//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "attribute_handler/attribute_codec.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "attribute_handler/classification_attribute_handler.h"
#include "attribute_handler/position_attribute_handler.h"
#include "attribute_handler/rgb_attribute_handler.h"
#include "attribute_handler/scalar_attribute_handler.h"
#include "attributes.h"

namespace {

constexpr double kMax = std::numeric_limits<double>::max();
constexpr double kLowest = std::numeric_limits<double>::lowest();

Attribute makePositionAttr(vec3d scale, vec3d offset) {
    Attribute a;
    a.name_ = "position";
    a.numElements_ = 3;
    a.bytes_ = 12;
    a.type_ = AttributeType::INT32;
    a.scale_ = scale;
    a.offset_ = offset;
    a.min_ = {kMax, kMax, kMax};
    a.max_ = {kLowest, kLowest, kLowest};
    return a;
}

Attribute makeScalarAttr(const std::string &name, AttributeType type, int bytes) {
    Attribute a;
    a.name_ = name;
    a.numElements_ = 1;
    a.bytes_ = bytes;
    a.type_ = type;
    a.scale_ = {1, 1, 1};
    a.offset_ = {0, 0, 0};
    a.min_ = {kMax, kMax, kMax};
    a.max_ = {kLowest, kLowest, kLowest};
    return a;
}

Attribute makeRgbAttr() {
    Attribute a;
    a.name_ = "rgb";
    a.numElements_ = 3;
    a.bytes_ = 6;
    a.type_ = AttributeType::UINT16;
    a.scale_ = {1, 1, 1};
    a.offset_ = {0, 0, 0};
    a.min_ = {kMax, kMax, kMax};
    a.max_ = {kLowest, kLowest, kLowest};
    return a;
}

} // namespace

TEST(AttributeCodec, WriteElementClampsToTypeRange) {
    uint8_t buf[1];
    attribute_codec::writeElement(buf, AttributeType::UINT8, 300.0);
    EXPECT_EQ(buf[0], 255);

    attribute_codec::writeElement(buf, AttributeType::UINT8, -10.0);
    EXPECT_EQ(buf[0], 0);
}

TEST(PositionAttributeHandler, EncodeRequantizesAcrossScaleOffset) {
    auto srcAttr = makePositionAttr({0.001, 0.001, 0.001}, {0, 0, 0});
    auto dstAttr = makePositionAttr({0.001, 0.001, 0.001}, {10, 20, -1});

    int32_t srcRaw[3] = {12345, 67890, -100};
    uint8_t srcBytes[12];
    std::memcpy(srcBytes, srcRaw, sizeof(srcRaw));

    uint8_t dstBytes[12];
    PositionAttributeHandler handler;
    handler.encode(dstBytes, srcBytes, srcAttr, dstAttr);

    int32_t dstRaw[3];
    std::memcpy(dstRaw, dstBytes, sizeof(dstRaw));
    EXPECT_EQ(dstRaw[0], 2345);
    EXPECT_EQ(dstRaw[1], 47890);
    EXPECT_EQ(dstRaw[2], 900);

    double world[3];
    handler.decode(dstBytes, dstAttr, world);
    EXPECT_NEAR(world[0], 12.345, 1e-9);
    EXPECT_NEAR(world[1], 67.89, 1e-9);
    EXPECT_NEAR(world[2], -0.1, 1e-9);
}

TEST(PositionAttributeHandler, UpdateStatsExpandsBoundingBox) {
    auto attr = makePositionAttr({1, 1, 1}, {0, 0, 0});

    int32_t rows[2][3] = {{1, 2, 3}, {-5, 10, 0}};
    uint8_t buf[24];
    std::memcpy(buf, rows, sizeof(rows));

    PositionAttributeHandler handler;
    handler.updateStats(attr, buf, 2, 12);

    EXPECT_DOUBLE_EQ(attr.min_.x, -5);
    EXPECT_DOUBLE_EQ(attr.min_.y, 2);
    EXPECT_DOUBLE_EQ(attr.min_.z, 0);
    EXPECT_DOUBLE_EQ(attr.max_.x, 1);
    EXPECT_DOUBLE_EQ(attr.max_.y, 10);
    EXPECT_DOUBLE_EQ(attr.max_.z, 3);
}

TEST(PositionAttributeHandler, MergeCopiesSelectedRow) {
    auto attr = makePositionAttr({1, 1, 1}, {0, 0, 0});

    int32_t row0[3] = {1, 2, 3};
    int32_t row1[3] = {4, 5, 6};
    uint8_t dst[12] = {0};
    std::vector<const uint8_t *> srcs = {
        reinterpret_cast<const uint8_t *>(row0),
        reinterpret_cast<const uint8_t *>(row1),
    };

    PositionAttributeHandler handler;
    handler.merge(dst, srcs, 1, attr);

    int32_t result[3];
    std::memcpy(result, dst, sizeof(result));
    EXPECT_EQ(result[0], 4);
    EXPECT_EQ(result[1], 5);
    EXPECT_EQ(result[2], 6);
}

TEST(ScalarAttributeHandler, EncodeIsIdentityForSameScaleOffset) {
    auto attr = makeScalarAttr("intensity", AttributeType::UINT16, 2);

    uint16_t srcVal = 12345;
    uint8_t srcBytes[2];
    std::memcpy(srcBytes, &srcVal, sizeof(srcVal));

    uint8_t dstBytes[2];
    ScalarAttributeHandler handler;
    handler.encode(dstBytes, srcBytes, attr, attr);

    uint16_t dstVal;
    std::memcpy(&dstVal, dstBytes, sizeof(dstVal));
    EXPECT_EQ(dstVal, 12345);

    double out[1];
    handler.decode(dstBytes, attr, out);
    EXPECT_DOUBLE_EQ(out[0], 12345.0);
}

TEST(ScalarAttributeHandler, UpdateStatsTracksMinMax) {
    auto attr = makeScalarAttr("intensity", AttributeType::UINT16, 2);

    uint16_t values[3] = {10, 500, 250};
    uint8_t buf[6];
    std::memcpy(buf, values, sizeof(values));

    ScalarAttributeHandler handler;
    handler.updateStats(attr, buf, 3, 2);

    EXPECT_DOUBLE_EQ(attr.min_.x, 10.0);
    EXPECT_DOUBLE_EQ(attr.max_.x, 500.0);
}

TEST(ScalarAttributeHandler, MergeCopiesSelectedRow) {
    auto attr = makeScalarAttr("intensity", AttributeType::UINT16, 2);

    uint16_t row0 = 11, row1 = 22;
    uint8_t dst[2] = {0, 0};
    std::vector<const uint8_t *> srcs = {
        reinterpret_cast<const uint8_t *>(&row0),
        reinterpret_cast<const uint8_t *>(&row1),
    };

    ScalarAttributeHandler handler;
    handler.merge(dst, srcs, 0, attr);

    uint16_t result;
    std::memcpy(&result, dst, sizeof(result));
    EXPECT_EQ(result, 11);
}

TEST(ClassificationAttributeHandler, UpdateStatsBuildsHistogramAndRange) {
    auto attr = makeScalarAttr("classification", AttributeType::UINT8, 1);
    ASSERT_EQ(attr.histogram_.size(), 256u);

    uint8_t values[4] = {0, 5, 5, 250};

    ClassificationAttributeHandler handler;
    handler.updateStats(attr, values, 4, 1);

    EXPECT_EQ(attr.histogram_[0], 1);
    EXPECT_EQ(attr.histogram_[5], 2);
    EXPECT_EQ(attr.histogram_[250], 1);
    EXPECT_DOUBLE_EQ(attr.min_.x, 0.0);
    EXPECT_DOUBLE_EQ(attr.max_.x, 250.0);
}

TEST(RGBAttributeHandler, EncodeDecodeRoundTrip) {
    auto attr = makeRgbAttr();

    uint16_t srcRow[3] = {10, 20, 30};
    uint8_t srcBytes[6];
    std::memcpy(srcBytes, srcRow, sizeof(srcRow));

    uint8_t dstBytes[6];
    RGBAttributeHandler handler;
    handler.encode(dstBytes, srcBytes, attr, attr);

    uint16_t dstRow[3];
    std::memcpy(dstRow, dstBytes, sizeof(dstRow));
    EXPECT_EQ(dstRow[0], 10);
    EXPECT_EQ(dstRow[1], 20);
    EXPECT_EQ(dstRow[2], 30);

    double out[3];
    handler.decode(dstBytes, attr, out);
    EXPECT_DOUBLE_EQ(out[0], 10.0);
    EXPECT_DOUBLE_EQ(out[1], 20.0);
    EXPECT_DOUBLE_EQ(out[2], 30.0);
}

TEST(RGBAttributeHandler, UpdateStatsTracksPerChannelRange) {
    auto attr = makeRgbAttr();

    uint16_t row0[3] = {100, 200, 300};
    uint16_t row1[3] = {50, 600, 700};
    uint8_t buf[12];
    std::memcpy(buf, row0, sizeof(row0));
    std::memcpy(buf + 6, row1, sizeof(row1));

    RGBAttributeHandler handler;
    handler.updateStats(attr, buf, 2, 6);

    EXPECT_DOUBLE_EQ(attr.min_.x, 50);
    EXPECT_DOUBLE_EQ(attr.min_.y, 200);
    EXPECT_DOUBLE_EQ(attr.min_.z, 300);
    EXPECT_DOUBLE_EQ(attr.max_.x, 100);
    EXPECT_DOUBLE_EQ(attr.max_.y, 600);
    EXPECT_DOUBLE_EQ(attr.max_.z, 700);
}

TEST(RGBAttributeHandler, MergeFollowsSelectedIndex) {
    auto attr = makeRgbAttr();

    uint16_t row0[3] = {1, 2, 3};
    uint16_t row1[3] = {4, 5, 6};
    uint8_t dst[6] = {0};
    std::vector<const uint8_t *> srcs = {
        reinterpret_cast<const uint8_t *>(row0),
        reinterpret_cast<const uint8_t *>(row1),
    };

    RGBAttributeHandler handler;
    handler.merge(dst, srcs, 0, attr);

    uint16_t result[3];
    std::memcpy(result, dst, sizeof(result));
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}

TEST(AttributeHandlerRegistry, DispatchesByAttributeName) {
    Attribute position;
    position.name_ = "position";
    Attribute rgb;
    rgb.name_ = "rgb";
    Attribute classification;
    classification.name_ = "classification";
    Attribute intensity;
    intensity.name_ = "intensity";

    EXPECT_NE(dynamic_cast<PositionAttributeHandler *>(AttributeHandlerRegistry::get(position)), nullptr);
    EXPECT_NE(dynamic_cast<RGBAttributeHandler *>(AttributeHandlerRegistry::get(rgb)), nullptr);
    EXPECT_NE(dynamic_cast<ClassificationAttributeHandler *>(AttributeHandlerRegistry::get(classification)), nullptr);

    auto *intensityHandler = AttributeHandlerRegistry::get(intensity);
    EXPECT_NE(dynamic_cast<ScalarAttributeHandler *>(intensityHandler), nullptr);
    EXPECT_EQ(dynamic_cast<ClassificationAttributeHandler *>(intensityHandler), nullptr);
    EXPECT_EQ(dynamic_cast<PositionAttributeHandler *>(intensityHandler), nullptr);
}
