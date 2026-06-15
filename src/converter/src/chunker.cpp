//
// Created by cj on 2026-05-13.
//

#include "chunker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "attribute_handler/attribute_handler.h"
#include "attribute_handler/attribute_handler_registry.h"
#include "attribute_handler/classification_attribute_handler.h"
#include "attribute_reader/attribute_reader.h"
#include "concurrent_writer.h"
#include "octree_naming.h"

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

uint64_t flattenIndex(uint64_t x, uint64_t y, uint64_t z, uint64_t size) {
    return x + y * size + z * size * size;
}

std::string attributeTypeName(AttributeType type) {
    switch (type) {
        case AttributeType::INT8:   return "int8";
        case AttributeType::INT16:  return "int16";
        case AttributeType::INT32:  return "int32";
        case AttributeType::INT64:  return "int64";
        case AttributeType::UINT8:  return "uint8";
        case AttributeType::UINT16: return "uint16";
        case AttributeType::UINT32: return "uint32";
        case AttributeType::UINT64: return "uint64";
        case AttributeType::FLOAT:  return "float";
        case AttributeType::DOUBLE: return "double";
        default: return "undefined";
    }
}

nlohmann::json vecToJson(const vec3d &v, int numElements) {
    nlohmann::json arr = nlohmann::json::array();
    if (numElements >= 1) arr.push_back(v.x);
    if (numElements >= 2) arr.push_back(v.y);
    if (numElements >= 3) arr.push_back(v.z);
    return arr;
}

nlohmann::json attributeToJson(const Attribute &attr) {
    nlohmann::json j;
    j["name"] = attr.name_;
    j["description"] = attr.description_;
    j["size"] = attr.bytes_;
    j["numElements"] = attr.numElements_;
    j["elementSize"] = attr.numElements_ > 0 ? attr.bytes_ / attr.numElements_ : attr.bytes_;
    j["type"] = attributeTypeName(attr.type_);

    if (dynamic_cast<ClassificationAttributeHandler *>(AttributeHandlerRegistry::get(attr)) != nullptr) {
        j["histogram"] = attr.histogram_;
    }

    j["min"] = vecToJson(attr.min_, attr.numElements_);
    j["max"] = vecToJson(attr.max_, attr.numElements_);
    j["scale"] = vecToJson(attr.scale_, attr.numElements_);
    j["offset"] = vecToJson(attr.offset_, attr.numElements_);
    return j;
}

} // namespace

Chunker::Chunker(std::vector<std::shared_ptr<AttributeReader>> readers,
                 Attributes attributes,
                 std::string targetDir,
                 std::shared_ptr<ConcurrentWriter> writer,
                 ConverterOptions options)
    : readers_(std::move(readers)),
      attributes_(std::move(attributes)),
      targetDir_(std::move(targetDir)),
      writer_(std::move(writer)),
      options_(std::move(options)) {}

bool Chunker::run() {
    aabb_ = computeAABB();
    if (!aabb_.isValid()) return false;

    // Rebase position offset to the dataset origin so on-disk coordinates are
    // small integers (matches the 2.1 metadata.json reference layout).
    for (auto &attr : attributes_) {
        if (attr.name_ == "position") {
            attr.offset_ = aabb_.min();
        }
    }

    uint64_t totalPoints = 0;
    for (auto &reader : readers_) {
        auto info = reader->headerInfo();
        totalPoints += std::max<uint64_t>(info.numPoints_, info.extendedNumPoints_);
    }

    gridSize_ = calculateGridSize(totalPoints);

    uint64_t maxPointsPerChunk = options_.maxPointsPerChunk;
    if (maxPointsPerChunk == 0) {
        maxPointsPerChunk = std::min<uint64_t>(totalPoints / 20, 10'000'000);
    }

    std::vector<int64_t> grid = countPointsInCells(gridSize_);

    std::vector<int32_t> lut;
    buildChunksAndLut(grid, gridSize_, maxPointsPerChunk, chunks_, lut);

    if (!distributePoints(lut, gridSize_)) return false;

    return writeMetadata();
}

BoundingBoxd Chunker::computeAABB() const {
    if (readers_.empty()) return BoundingBoxd();

    vec3d lo{kInf, kInf, kInf};
    vec3d hi{-kInf, -kInf, -kInf};

    for (auto &reader : readers_) {
        auto info = reader->headerInfo();
        lo.x = std::min(lo.x, info.min_.x);
        lo.y = std::min(lo.y, info.min_.y);
        lo.z = std::min(lo.z, info.min_.z);
        hi.x = std::max(hi.x, info.max_.x);
        hi.y = std::max(hi.y, info.max_.y);
        hi.z = std::max(hi.z, info.max_.z);
    }

    double maxSide = std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
    vec3d cubeMax{lo.x + maxSide, lo.y + maxSide, lo.z + maxSide};

    return BoundingBoxd(lo, cubeMax);
}

uint32_t Chunker::calculateGridSize(uint64_t totalPoints) const {
    uint32_t gridSize;
    if (totalPoints < 100'000'000ull) gridSize = 128;
    else if (totalPoints < 500'000'000ull) gridSize = 256;
    else gridSize = 512;

    return std::max(gridSize, options_.minGridSize);
}

std::vector<int64_t> Chunker::countPointsInCells(uint32_t gridSize) const {
    uint64_t numCells = static_cast<uint64_t>(gridSize) * gridSize * gridSize;
    std::vector<int64_t> grid(numCells, 0);

    constexpr int64_t kBatchSize = 1'000'000;

    for (auto &reader : readers_) {
        auto info = reader->headerInfo();
        uint64_t total = std::max<uint64_t>(info.numPoints_, info.extendedNumPoints_);

        for (uint64_t start = 0; start < total; start += kBatchSize) {
            int64_t count = static_cast<int64_t>(std::min<uint64_t>(kBatchSize, total - start));
            std::vector<vec3d> positions = reader->readPositions(start, count);

            for (const vec3d &p : positions) {
                uint64_t idx = octree_naming::cellIndex(p, aabb_, gridSize);
                ++grid[idx];
            }
        }
    }

    return grid;
}

void Chunker::buildChunksAndLut(const std::vector<int64_t> &grid, uint32_t gridSize,
                                 uint64_t maxPointsPerChunk,
                                 std::vector<ChunkInfo> &outChunks,
                                 std::vector<int32_t> &outLut) const {
    outChunks.clear();

    int levelMax = static_cast<int>(std::round(std::log2(static_cast<double>(gridSize))));

    vec3d aabbMin = aabb_.min();
    double aabbEdge = aabb_.getSize().x;

    auto pushChunk = [&](int level, uint32_t x, uint32_t y, uint32_t z) {
        ChunkInfo info;
        info.level = level;
        info.x = x;
        info.y = y;
        info.z = z;
        info.size = 1u << (levelMax - level);
        info.numPoints = 0;
        info.name = octree_naming::toNodeID(level, 1u << level, x, y, z);

        double cellSize = aabbEdge / static_cast<double>(1u << level);
        vec3d cmin{aabbMin.x + x * cellSize, aabbMin.y + y * cellSize, aabbMin.z + z * cellSize};
        vec3d cmax{cmin.x + cellSize, cmin.y + cellSize, cmin.z + cellSize};
        info.bb = BoundingBoxd(cmin, cmax);

        outChunks.push_back(std::move(info));
    };

    std::vector<int64_t> gridHigh = grid;

    for (int levelLow = levelMax - 1; levelLow >= 0; --levelLow) {
        int levelHigh = levelLow + 1;
        uint32_t sizeLow = 1u << levelLow;
        uint32_t sizeHigh = 1u << levelHigh;

        std::vector<int64_t> gridLow(static_cast<size_t>(sizeLow) * sizeLow * sizeLow, 0);

        for (uint32_t z = 0; z < sizeLow; ++z) {
            for (uint32_t y = 0; y < sizeLow; ++y) {
                for (uint32_t x = 0; x < sizeLow; ++x) {
                    uint64_t idxLow = flattenIndex(x, y, z, sizeLow);
                    std::array<uint64_t, 8> children = octree_naming::subdividedIndices(x, y, z, sizeLow);

                    int64_t sum = 0;
                    bool finalized = false;
                    for (uint64_t c : children) {
                        int64_t v = gridHigh[c];
                        if (v == -1) finalized = true;
                        else sum += v;
                    }

                    if (finalized || sum > static_cast<int64_t>(maxPointsPerChunk)) {
                        for (uint64_t c : children) {
                            int64_t v = gridHigh[c];
                            if (v > 0) {
                                uint64_t cx = c % sizeHigh;
                                uint64_t cy = (c / sizeHigh) % sizeHigh;
                                uint64_t cz = c / (static_cast<uint64_t>(sizeHigh) * sizeHigh);
                                pushChunk(levelHigh, static_cast<uint32_t>(cx),
                                          static_cast<uint32_t>(cy), static_cast<uint32_t>(cz));
                            }
                        }
                        gridLow[idxLow] = -1;
                    } else {
                        gridLow[idxLow] = sum;
                    }
                }
            }
        }

        gridHigh = std::move(gridLow);
    }

    if (gridHigh[0] > 0) {
        pushChunk(0, 0, 0, 0);
    }

    outLut.assign(static_cast<size_t>(gridSize) * gridSize * gridSize, -1);
    for (size_t i = 0; i < outChunks.size(); ++i) {
        const ChunkInfo &chunk = outChunks[i];
        for (uint32_t oz = 0; oz < chunk.size; ++oz) {
            for (uint32_t oy = 0; oy < chunk.size; ++oy) {
                for (uint32_t ox = 0; ox < chunk.size; ++ox) {
                    uint32_t x = chunk.size * chunk.x + ox;
                    uint32_t y = chunk.size * chunk.y + oy;
                    uint32_t z = chunk.size * chunk.z + oz;
                    outLut[flattenIndex(x, y, z, gridSize)] = static_cast<int32_t>(i);
                }
            }
        }
    }
}

bool Chunker::distributePoints(const std::vector<int32_t> &lut, uint32_t gridSize) {
    for (auto &attr : attributes_) {
        attr.min_ = {kInf, kInf, kInf};
        attr.max_ = {-kInf, -kInf, -kInf};
    }

    uint64_t dstRowBytes = attributes_.getTotalBytes();
    constexpr int64_t kBatchSize = 1'000'000;

    struct AttributePlan {
        Attribute srcAttr;
        uint64_t srcOffset;
        uint64_t dstOffset;
        AttributeHandler *handler;
        Attribute *dstAttr;
    };

    for (auto &reader : readers_) {
        Attributes srcAttrs = reader->getAttributes();
        uint64_t srcRowBytes = srcAttrs.getTotalBytes();

        Attribute srcPosAttr = srcAttrs.getAttribute("position");
        uint64_t srcPosOffset = srcAttrs.getOffset("position");
        AttributeHandler *posHandler = AttributeHandlerRegistry::get(srcPosAttr);

        std::vector<AttributePlan> plans;
        plans.reserve(attributes_.size());
        for (auto &dstAttr : attributes_) {
            AttributePlan plan;
            plan.srcAttr = srcAttrs.getAttribute(dstAttr.name_);
            plan.srcOffset = srcAttrs.getOffset(dstAttr.name_);
            plan.dstOffset = attributes_.getOffset(dstAttr.name_);
            plan.handler = AttributeHandlerRegistry::get(dstAttr);
            plan.dstAttr = &dstAttr;
            plans.push_back(plan);
        }

        auto info = reader->headerInfo();
        uint64_t total = std::max<uint64_t>(info.numPoints_, info.extendedNumPoints_);

        for (uint64_t start = 0; start < total; start += kBatchSize) {
            int64_t count = static_cast<int64_t>(std::min<uint64_t>(kBatchSize, total - start));
            std::vector<uint8_t> rawRows = reader->readRawData(start, count);

            std::vector<uint8_t> dstBuf(static_cast<size_t>(count) * dstRowBytes);
            std::vector<int32_t> chunkIdx(static_cast<size_t>(count));

            for (int64_t i = 0; i < count; ++i) {
                const uint8_t *srcRow = rawRows.data() + static_cast<uint64_t>(i) * srcRowBytes;
                uint8_t *dstRow = dstBuf.data() + static_cast<uint64_t>(i) * dstRowBytes;

                double pos[3];
                posHandler->decode(srcRow + srcPosOffset, srcPosAttr, pos);
                vec3d p{pos[0], pos[1], pos[2]};

                uint64_t cell = octree_naming::cellIndex(p, aabb_, gridSize);
                chunkIdx[i] = lut[cell];

                for (auto &plan : plans) {
                    plan.handler->encode(dstRow + plan.dstOffset, srcRow + plan.srcOffset,
                                          plan.srcAttr, *plan.dstAttr);
                }
            }

            for (auto &plan : plans) {
                plan.handler->updateStats(*plan.dstAttr, dstBuf.data() + plan.dstOffset,
                                           static_cast<uint64_t>(count), dstRowBytes);
            }

            std::unordered_map<int32_t, std::vector<uint8_t>> perChunk;
            for (int64_t i = 0; i < count; ++i) {
                int32_t idx = chunkIdx[i];
                const uint8_t *row = dstBuf.data() + static_cast<uint64_t>(i) * dstRowBytes;
                std::vector<uint8_t> &buf = perChunk[idx];
                buf.insert(buf.end(), row, row + dstRowBytes);
                chunks_[idx].numPoints += 1;
            }

            for (auto &[idx, buf] : perChunk) {
                writer_->append("chunks/" + chunks_[idx].name + ".bin", buf);
            }
        }
    }

    return true;
}

bool Chunker::writeMetadata() const {
    nlohmann::json j;

    j["version"] = "2.0";

    std::string name = "pointcloud";
    if (!readers_.empty()) {
        name = std::filesystem::path(readers_[0]->headerInfo().name_).stem().string();
    }
    j["name"] = name;
    j["description"] = "";

    uint64_t totalPoints = 0;
    for (const ChunkInfo &chunk : chunks_) totalPoints += chunk.numPoints;
    j["points"] = totalPoints;

    j["projection"] = "";

    vec3d aMin = aabb_.min();
    vec3d aMax = aabb_.max();
    j["offset"] = {aMin.x, aMin.y, aMin.z};

    vec3d posScale{1, 1, 1};
    for (const Attribute &attr : attributes_) {
        if (attr.name_ == "position") posScale = attr.scale_;
    }
    j["scale"] = {posScale.x, posScale.y, posScale.z};

    j["spacing"] = aabb_.getSize().x / static_cast<double>(gridSize_);

    j["boundingBox"] = {
        {"min", {aMin.x, aMin.y, aMin.z}},
        {"max", {aMax.x, aMax.y, aMax.z}},
    };

    j["encoding"] = "DEFAULT";

    j["hierarchy"] = {
        {"firstChunkSize", options_.firstChunkSize},
        {"stepSize", options_.stepSize},
        {"depth", 0},
    };

    nlohmann::json attrsJson = nlohmann::json::array();
    for (const Attribute &attr : attributes_) {
        attrsJson.push_back(attributeToJson(attr));
    }
    j["attributes"] = attrsJson;

    std::error_code ec;
    std::filesystem::create_directories(targetDir_, ec);

    std::ofstream out(std::filesystem::path(targetDir_) / "metadata.json", std::ios::binary);
    if (!out) return false;
    out << j.dump(4);
    return static_cast<bool>(out);
}
