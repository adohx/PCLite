//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <random>
#include <vector>

#include "attribute_handler/attribute_codec.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "concurrent_writer.h"
#include "sampler.h"

namespace {

Attribute makePositionAttribute(vec3d scale) {
    Attribute position;
    position.name_ = "position";
    position.numElements_ = 3;
    position.bytes_ = 12;
    position.type_ = AttributeType::INT32;
    position.scale_ = scale;
    position.offset_ = {0, 0, 0};
    return position;
}

void encodePosition(std::vector<uint8_t> &buf, size_t rowOffset, const vec3d &p, const Attribute &posAttr) {
    for (int i = 0; i < 3; ++i) {
        double comp = attribute_codec::component(p, i);
        double scale = attribute_codec::component(posAttr.scale_, i);
        attribute_codec::writeElement(buf.data() + rowOffset + i * 4, posAttr.type_, comp / scale);
    }
}

vec3d decodePosition(const uint8_t *row, const Attribute &posAttr) {
    double out[3] = {0, 0, 0};
    AttributeHandlerRegistry::get(posAttr)->decode(row, posAttr, out);
    return {out[0], out[1], out[2]};
}

double distance(const vec3d &a, const vec3d &b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void expectPoissonInvariant(const std::vector<uint8_t> &accepted, const std::vector<uint8_t> &rejected,
                             const Attribute &posAttr, uint64_t rowStride, double spacing) {
    std::vector<vec3d> acceptedPositions;
    for (size_t off = 0; off < accepted.size(); off += rowStride) {
        acceptedPositions.push_back(decodePosition(accepted.data() + off, posAttr));
    }

    for (size_t i = 0; i < acceptedPositions.size(); ++i) {
        for (size_t j = i + 1; j < acceptedPositions.size(); ++j) {
            EXPECT_GE(distance(acceptedPositions[i], acceptedPositions[j]), spacing - 1e-9)
                << "accepted points " << i << " and " << j << " are closer than spacing";
        }
    }

    for (size_t off = 0; off < rejected.size(); off += rejected.empty() ? 1 : rowStride) {
        vec3d p = decodePosition(rejected.data() + off, posAttr);
        bool hasCloseAccepted = false;
        for (const vec3d &a : acceptedPositions) {
            if (distance(p, a) < spacing + 1e-9) {
                hasCloseAccepted = true;
                break;
            }
        }
        EXPECT_TRUE(hasCloseAccepted) << "rejected point has no nearby accepted point";
    }
}

} // namespace

class SamplerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "pclite_sampler_test";
        std::filesystem::create_directories(dir_);
        writer_ = std::make_shared<ConcurrentWriter>(dir_.string());
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    std::unique_ptr<Sampler> makePoissonSampler(const Attributes &attrs) {
        return createSampler("poisson", writer_, dir_.string(), attrs, ConverterOptions{});
    }

    std::shared_ptr<Node> makeDummyNode(const BoundingBoxd &bb) {
        return std::make_shared<Node>("r", bb, nullptr);
    }

    std::filesystem::path dir_;
    std::shared_ptr<ConcurrentWriter> writer_;
};

TEST_F(SamplerTest, PoissonDisk_LinearChainAcceptsEverySecondPoint) {
    Attribute posAttr = makePositionAttribute({0.01, 0.01, 0.01});
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    std::vector<vec3d> positions = {
        {0.0, 0, 0}, {0.4, 0, 0}, {0.8, 0, 0}, {1.2, 0, 0}, {1.6, 0, 0}, {2.0, 0, 0},
    };

    std::vector<uint8_t> buf(positions.size() * 12);
    for (size_t i = 0; i < positions.size(); ++i)
        encodePosition(buf, i * 12, positions[i], posAttr);

    PointBatch batch{buf.data(), positions.size(), 12};
    auto sampler = makePoissonSampler(attrs);
    auto node    = makeDummyNode(BoundingBoxd({0, 0, 0}, {2, 2, 2}));

    std::vector<uint8_t> accepted, rejected, flags;
    sampler->doSample(batch, node, 1.0, accepted, rejected, flags);

    EXPECT_EQ(accepted.size() / 12, 2u);
    EXPECT_EQ(rejected.size() / 12, 4u);
    expectPoissonInvariant(accepted, rejected, posAttr, 12, 1.0);
}

TEST_F(SamplerTest, PoissonDisk_RandomPointsSatisfyPoissonInvariant) {
    Attribute posAttr = makePositionAttribute({0.0001, 0.0001, 0.0001});
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 10.0);

    std::vector<vec3d> positions;
    for (int i = 0; i < 300; ++i)
        positions.push_back({dist(rng), dist(rng), dist(rng)});

    std::vector<uint8_t> buf(positions.size() * 12);
    for (size_t i = 0; i < positions.size(); ++i)
        encodePosition(buf, i * 12, positions[i], posAttr);

    PointBatch batch{buf.data(), positions.size(), 12};
    auto sampler = makePoissonSampler(attrs);
    auto node    = makeDummyNode(BoundingBoxd({0, 0, 0}, {10, 10, 10}));

    std::vector<uint8_t> accepted, rejected, flags;
    sampler->doSample(batch, node, 0.5, accepted, rejected, flags);

    EXPECT_EQ(accepted.size() + rejected.size(), buf.size());
    EXPECT_GT(accepted.size(), 0u);
    expectPoissonInvariant(accepted, rejected, posAttr, 12, 0.5);
}

TEST_F(SamplerTest, PoissonDisk_EmptyCandidatesProduceEmptyOutputs) {
    Attribute posAttr = makePositionAttribute({0.01, 0.01, 0.01});
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    PointBatch batch{nullptr, 0, 12};
    auto sampler = makePoissonSampler(attrs);
    auto node    = makeDummyNode(BoundingBoxd({0, 0, 0}, {1, 1, 1}));

    std::vector<uint8_t> accepted, rejected, flags;
    sampler->doSample(batch, node, 0.5, accepted, rejected, flags);

    EXPECT_TRUE(accepted.empty());
    EXPECT_TRUE(rejected.empty());
}

TEST_F(SamplerTest, PoissonDisk_NonPositiveSpacingAcceptsAllPoints) {
    Attribute posAttr = makePositionAttribute({0.01, 0.01, 0.01});
    Attributes attrs;
    attrs.pushAttribute(posAttr);

    std::vector<vec3d> positions = {{0, 0, 0}, {0.01, 0, 0}, {0.02, 0, 0}};
    std::vector<uint8_t> buf(positions.size() * 12);
    for (size_t i = 0; i < positions.size(); ++i)
        encodePosition(buf, i * 12, positions[i], posAttr);

    PointBatch batch{buf.data(), positions.size(), 12};
    auto sampler = makePoissonSampler(attrs);
    auto node    = makeDummyNode(BoundingBoxd({0, 0, 0}, {1, 1, 1}));

    std::vector<uint8_t> accepted, rejected, flags;
    sampler->doSample(batch, node, 0.0, accepted, rejected, flags);

    EXPECT_EQ(accepted.size(), buf.size());
    EXPECT_TRUE(rejected.empty());
}
