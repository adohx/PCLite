//
// Created by cj on 2026-06-14.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

#include "concurrent_writer.h"

namespace {

std::vector<uint8_t> makeBytes(size_t size, uint8_t fill) {
    return std::vector<uint8_t>(size, fill);
}

std::vector<uint8_t> readFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

class ConcurrentWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("pclite_concurrent_writer_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }

    std::filesystem::path dir_;
};

TEST_F(ConcurrentWriterTest, SequentialAppendReturnsCorrectOffsetsAndContent) {
    ConcurrentWriter writer(dir_.string(), 2);

    auto a = makeBytes(10, 0xAA);
    auto b = makeBytes(20, 0xBB);
    auto c = makeBytes(5, 0xCC);

    auto ra = writer.append("data.bin", a);
    auto rb = writer.append("data.bin", b);
    auto rc = writer.append("data.bin", c);

    EXPECT_EQ(ra.offset, 0u);
    EXPECT_EQ(ra.size, 10u);
    EXPECT_EQ(rb.offset, 10u);
    EXPECT_EQ(rb.size, 20u);
    EXPECT_EQ(rc.offset, 30u);
    EXPECT_EQ(rc.size, 5u);

    writer.flushAll();

    auto content = readFile(dir_ / "data.bin");
    ASSERT_EQ(content.size(), 35u);
    EXPECT_TRUE(std::all_of(content.begin(), content.begin() + 10, [](uint8_t v) { return v == 0xAA; }));
    EXPECT_TRUE(std::all_of(content.begin() + 10, content.begin() + 30, [](uint8_t v) { return v == 0xBB; }));
    EXPECT_TRUE(std::all_of(content.begin() + 30, content.end(), [](uint8_t v) { return v == 0xCC; }));
}

TEST_F(ConcurrentWriterTest, ConcurrentAppendsToSameFileAreNonOverlapping) {
    ConcurrentWriter writer(dir_.string(), 8);

    constexpr int kTasks = 50;
    constexpr size_t kChunkSize = 16;

    std::vector<std::future<ConcurrentWriter::WriteResult>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i) {
        // Each chunk is filled with a distinct byte value so we can verify
        // that the bytes at the returned offset really belong to this task.
        futures.push_back(writer.appendAsync("shared.bin", makeBytes(kChunkSize, static_cast<uint8_t>(i))));
    }

    std::vector<ConcurrentWriter::WriteResult> results;
    results.reserve(kTasks);
    for (auto &f : futures) {
        results.push_back(f.get());
    }

    writer.flushAll();

    auto content = readFile(dir_ / "shared.bin");
    ASSERT_EQ(content.size(), kTasks * kChunkSize);

    // Offsets must tile [0, total) without gaps or overlaps.
    std::vector<uint64_t> offsets;
    offsets.reserve(kTasks);
    for (auto &r : results) {
        EXPECT_EQ(r.size, kChunkSize);
        offsets.push_back(r.offset);
    }
    std::sort(offsets.begin(), offsets.end());
    for (int i = 0; i < kTasks; ++i) {
        EXPECT_EQ(offsets[i], static_cast<uint64_t>(i) * kChunkSize);
    }

    // Each task's bytes must appear at its reported offset.
    for (int i = 0; i < kTasks; ++i) {
        const auto &r = results[i];
        for (uint64_t j = 0; j < r.size; ++j) {
            EXPECT_EQ(content[r.offset + j], static_cast<uint8_t>(i));
        }
    }
}

TEST_F(ConcurrentWriterTest, ConcurrentAppendsToDifferentFilesAreIndependent) {
    ConcurrentWriter writer(dir_.string(), 4);

    constexpr int kFiles = 6;
    constexpr int kChunksPerFile = 10;
    constexpr size_t kChunkSize = 8;

    std::vector<std::future<ConcurrentWriter::WriteResult>> futures;
    for (int f = 0; f < kFiles; ++f) {
        for (int c = 0; c < kChunksPerFile; ++c) {
            std::string name = "file_" + std::to_string(f) + ".bin";
            futures.push_back(writer.appendAsync(name, makeBytes(kChunkSize, static_cast<uint8_t>(f))));
        }
    }
    for (auto &fut : futures) {
        fut.get();
    }

    writer.flushAll();

    for (int f = 0; f < kFiles; ++f) {
        auto content = readFile(dir_ / ("file_" + std::to_string(f) + ".bin"));
        ASSERT_EQ(content.size(), kChunksPerFile * kChunkSize);
        EXPECT_TRUE(std::all_of(content.begin(), content.end(),
                                 [f](uint8_t v) { return v == static_cast<uint8_t>(f); }));
    }
}

TEST_F(ConcurrentWriterTest, WriteAtOverwritesBytesInPlace) {
    ConcurrentWriter writer(dir_.string(), 2);

    auto initial = makeBytes(32, 0x11);
    auto r = writer.append("patched.bin", initial);
    EXPECT_EQ(r.offset, 0u);
    EXPECT_EQ(r.size, 32u);

    auto patch = makeBytes(4, 0x99);
    writer.writeAt("patched.bin", 10, patch);

    // Further appends should continue from the original cursor, not the patch end.
    auto tail = makeBytes(3, 0x22);
    auto rTail = writer.append("patched.bin", tail);
    EXPECT_EQ(rTail.offset, 32u);

    writer.flushAll();

    auto content = readFile(dir_ / "patched.bin");
    ASSERT_EQ(content.size(), 35u);
    for (size_t i = 0; i < 10; ++i) EXPECT_EQ(content[i], 0x11);
    for (size_t i = 10; i < 14; ++i) EXPECT_EQ(content[i], 0x99);
    for (size_t i = 14; i < 32; ++i) EXPECT_EQ(content[i], 0x11);
    for (size_t i = 32; i < 35; ++i) EXPECT_EQ(content[i], 0x22);
}

TEST_F(ConcurrentWriterTest, FlushAllMakesDataReadableFromDisk) {
    ConcurrentWriter writer(dir_.string(), 2);

    auto data = makeBytes(100, 0x42);
    writer.append("flushed.bin", data);

    auto path = dir_ / "flushed.bin";
    // Before flushAll the file may not yet reflect the written bytes on disk.
    writer.flushAll();

    ASSERT_TRUE(std::filesystem::exists(path));
    auto content = readFile(path);
    EXPECT_EQ(content, data);
}
