//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "converter.h"

namespace {

void writeRaw(std::ofstream &out, const void *data, size_t bytes) {
    out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(bytes));
}

template <typename T>
void writeValue(std::ofstream &out, T value) {
    writeRaw(out, &value, sizeof(T));
}

void writePadded(std::ofstream &out, const std::string &s, size_t fieldSize) {
    std::vector<char> buf(fieldSize, '\0');
    std::memcpy(buf.data(), s.data(), std::min(s.size(), fieldSize));
    writeRaw(out, buf.data(), fieldSize);
}

// Hand-writes a minimal uncompressed LAS 1.2, point-data-format-2 (26 bytes
// per point) file: 227-byte header followed by N point records, no VLRs.
void writeSyntheticLas(const std::filesystem::path &path, const std::vector<vec3d> &points, const vec3d &scale,
                        const vec3d &offset) {
    std::vector<int32_t> ix(points.size()), iy(points.size()), iz(points.size());
    vec3d minW{ std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };
    vec3d maxW{ -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };

    for (size_t i = 0; i < points.size(); ++i) {
        ix[i] = static_cast<int32_t>(std::llround((points[i].x - offset.x) / scale.x));
        iy[i] = static_cast<int32_t>(std::llround((points[i].y - offset.y) / scale.y));
        iz[i] = static_cast<int32_t>(std::llround((points[i].z - offset.z) / scale.z));

        vec3d world{ix[i] * scale.x + offset.x, iy[i] * scale.y + offset.y, iz[i] * scale.z + offset.z};
        minW.x = std::min(minW.x, world.x); minW.y = std::min(minW.y, world.y); minW.z = std::min(minW.z, world.z);
        maxW.x = std::max(maxW.x, world.x); maxW.y = std::max(maxW.y, world.y); maxW.z = std::max(maxW.z, world.z);
    }

    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());

    // --- Public header block (227 bytes for LAS 1.2) ---
    writeRaw(out, "LASF", 4);                  // File signature
    writeValue<uint16_t>(out, 0);              // File source ID
    writeValue<uint16_t>(out, 0);              // Global encoding
    writeValue<uint32_t>(out, 0);              // GUID data 1
    writeValue<uint16_t>(out, 0);              // GUID data 2
    writeValue<uint16_t>(out, 0);              // GUID data 3
    for (int i = 0; i < 8; ++i) writeValue<uint8_t>(out, 0); // GUID data 4
    writeValue<uint8_t>(out, 1);               // Version major
    writeValue<uint8_t>(out, 2);               // Version minor
    writePadded(out, "PCLite", 32);            // System identifier
    writePadded(out, "PCLiteTest", 32);        // Generating software
    writeValue<uint16_t>(out, 1);              // File creation day of year
    writeValue<uint16_t>(out, 2026);           // File creation year
    writeValue<uint16_t>(out, 227);            // Header size
    writeValue<uint32_t>(out, 227);            // Offset to point data
    writeValue<uint32_t>(out, 0);              // Number of VLRs
    writeValue<uint8_t>(out, 2);               // Point data format ID
    writeValue<uint16_t>(out, 26);             // Point data record length
    writeValue<uint32_t>(out, static_cast<uint32_t>(points.size())); // Number of point records
    for (int i = 0; i < 5; ++i) writeValue<uint32_t>(out, 0); // Number of points by return
    writeValue<double>(out, scale.x);
    writeValue<double>(out, scale.y);
    writeValue<double>(out, scale.z);
    writeValue<double>(out, offset.x);
    writeValue<double>(out, offset.y);
    writeValue<double>(out, offset.z);
    writeValue<double>(out, maxW.x);
    writeValue<double>(out, minW.x);
    writeValue<double>(out, maxW.y);
    writeValue<double>(out, minW.y);
    writeValue<double>(out, maxW.z);
    writeValue<double>(out, minW.z);

    // --- Point records (format 2, 26 bytes each) ---
    for (size_t i = 0; i < points.size(); ++i) {
        writeValue<int32_t>(out, ix[i]);
        writeValue<int32_t>(out, iy[i]);
        writeValue<int32_t>(out, iz[i]);
        writeValue<uint16_t>(out, 0);          // Intensity
        writeValue<uint8_t>(out, (1u << 3) | 1u); // 1 return of 1
        writeValue<uint8_t>(out, 2);           // Classification
        writeValue<int8_t>(out, 0);            // Scan angle rank
        writeValue<uint8_t>(out, 0);           // User data
        writeValue<uint16_t>(out, 0);          // Point source ID
        writeValue<uint16_t>(out, static_cast<uint16_t>(i % 256));       // R
        writeValue<uint16_t>(out, static_cast<uint16_t>((i * 3) % 256)); // G
        writeValue<uint16_t>(out, static_cast<uint16_t>((i * 7) % 256)); // B
    }
}

struct HierarchyRecord {
    uint8_t type;
    uint8_t childMask;
    uint32_t numPoints;
    uint64_t address;
    uint64_t byteSize;
};

std::vector<HierarchyRecord> parseHierarchy(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::vector<HierarchyRecord> records;
    for (size_t off = 0; off + 22 <= bytes.size(); off += 22) {
        HierarchyRecord rec;
        rec.type = bytes[off + 0];
        rec.childMask = bytes[off + 1];
        std::memcpy(&rec.numPoints, bytes.data() + off + 2, 4);
        std::memcpy(&rec.address, bytes.data() + off + 6, 8);
        std::memcpy(&rec.byteSize, bytes.data() + off + 14, 8);
        records.push_back(rec);
    }
    return records;
}

} // namespace

TEST(ConverterTest, ConvertsSyntheticLasAndProducesValidOutputs) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                 ("pclite_converter_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    std::filesystem::path lasPath = dir / "synthetic.las";
    std::filesystem::path target = dir / "out";

    constexpr size_t kNumPoints = 2000;
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(0.0, 100.0);

    std::vector<vec3d> points;
    points.reserve(kNumPoints);
    for (size_t i = 0; i < kNumPoints; ++i) {
        points.push_back({dist(rng), dist(rng), dist(rng)});
    }

    writeSyntheticLas(lasPath, points, {0.001, 0.001, 0.001}, {0, 0, 0});

    Converter::Options options;
    options.maxPointsPerChunk = 500;
    options.firstChunkSize = 100;

    Converter converter({lasPath.string()}, target.string(), options);
    ASSERT_TRUE(converter.run());

    ASSERT_TRUE(std::filesystem::exists(target / "metadata.json"));
    ASSERT_TRUE(std::filesystem::exists(target / "hierarchy.bin"));
    ASSERT_TRUE(std::filesystem::exists(target / "octree.bin"));

    std::ifstream metaIn(target / "metadata.json");
    nlohmann::json meta;
    metaIn >> meta;

    EXPECT_EQ(meta["points"].get<uint64_t>(), kNumPoints);

    uint64_t rowStride = 0;
    bool hasPosition = false;
    for (const auto &attr : meta["attributes"]) {
        rowStride += attr["size"].get<uint64_t>();
        if (attr["name"] == "position") hasPosition = true;
    }
    EXPECT_TRUE(hasPosition);
    EXPECT_EQ(rowStride, 27u);

    uint64_t octreeSize = std::filesystem::file_size(target / "octree.bin");
    uint64_t hierarchySize = std::filesystem::file_size(target / "hierarchy.bin");

    ASSERT_GT(hierarchySize, 0u);
    ASSERT_EQ(hierarchySize % 22u, 0u);
    EXPECT_EQ(meta["hierarchy"]["firstChunkSize"].get<uint64_t>(), hierarchySize);

    std::vector<HierarchyRecord> records = parseHierarchy(target / "hierarchy.bin");
    ASSERT_FALSE(records.empty());

    // BFS-reconstruct levels in the same order writeHierarchy() produced the
    // records, to cross-check metadata.hierarchy.depth.
    std::vector<int> level(records.size(), 0);
    std::vector<size_t> queue = {0};
    size_t next = 1;
    int maxLevel = 0;

    uint64_t totalPoints = 0;
    for (size_t i = 0; i < records.size(); ++i) {
        const HierarchyRecord &rec = records[i];

        EXPECT_LE(rec.type, 1u) << "node " << i; // Normal or Leaf only (no Proxy in this version)
        EXPECT_LE(rec.address + rec.byteSize, octreeSize) << "node " << i;
        EXPECT_EQ(rec.byteSize, static_cast<uint64_t>(rec.numPoints) * rowStride) << "node " << i;

        int popcount = std::popcount(static_cast<unsigned>(rec.childMask));
        EXPECT_EQ((rec.childMask == 0), (rec.type == 1)) << "node " << i; // childMask==0 <=> Leaf

        for (int c = 0; c < popcount; ++c) {
            ASSERT_LT(next, records.size());
            level[next] = level[i] + 1;
            ++next;
        }
        maxLevel = std::max(maxLevel, level[i]);

        totalPoints += rec.numPoints;
    }
    EXPECT_EQ(next, records.size());
    EXPECT_EQ(totalPoints, kNumPoints);
    EXPECT_EQ(meta["hierarchy"]["depth"].get<int>(), maxLevel);

    std::filesystem::remove_all(dir);
}

// Runs the full converter pipeline against the real office.las (~49M points,
// ~1.2GB). Not part of the regular suite (slow); run manually with
// --gtest_also_run_disabled_tests to sanity-check on real data.
TEST(ConverterTest, DISABLED_ConvertsOfficeLas) {
    std::string lasPath = std::string(TEST_DATA_DIR) + "/office.las";

    std::filesystem::path target = std::filesystem::temp_directory_path() / "pclite_office_convert_out";
    std::filesystem::remove_all(target);

    Converter::Options options;
    Converter converter({lasPath}, target.string(), options);
    ASSERT_TRUE(converter.run());

    ASSERT_TRUE(std::filesystem::exists(target / "metadata.json"));
    ASSERT_TRUE(std::filesystem::exists(target / "hierarchy.bin"));
    ASSERT_TRUE(std::filesystem::exists(target / "octree.bin"));

    EXPECT_GT(std::filesystem::file_size(target / "octree.bin"), 0u);
    EXPECT_GT(std::filesystem::file_size(target / "hierarchy.bin"), 0u);
}
