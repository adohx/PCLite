//
// Created by cj on 2026-05-13.
//

#include "chunker.h"

#include "attribute_reader.h"
#include "chunker.h"

#include <atomic>
#include <cmath>
#include <filesystem>

#include "boundary_box.h"
#include "../../utilities/pclite_thread_pool.h"
#include "attribute_reader/attribute_reader.h"

class Chunker::ChunkPrivate {
public:
    struct Parameters {
        uint32_t gridSize_;
        uint64_t maxPointsPerChunk_;
        vec3d min_;
        vec3d max_;
    };

public:
    ChunkPrivate(Chunker *parent) {
        parent_ = parent;
        for (auto &source: parent->sources_) {
            readers_.emplace_back();
        }
    }

    ~ChunkPrivate() {
        parent_ = nullptr;
    }

    uint32_t calculateGridSize() {
        uint32_t gridSize = 128;

        uint64_t totalPoints = 0;
        for (const auto &reader: readers_) {
            totalPoints += reader->headerInfo().numPoints_;
        }

        const auto tmp = totalPoints / 20;
        maxPointsPerChunk_ = std::min(tmp, static_cast<uint64_t>(10'000'000));

        if (totalPoints < 100'000'000) {
            gridSize = 128;
        } else if (totalPoints < 500'000'000) {
            gridSize = 256;
        } else {
            gridSize = 512;
        }
        return gridSize;
    }

    [[nodiscard]] std::vector<std::atomic_int32_t> countPointsInCells(uint32_t gridSize, BoundaryBoxd bb) const {
        std::vector<std::atomic_int32_t> grid(gridSize * gridSize * gridSize);
        auto countTask = [this,gridSize, &grid,bb](const std::unique_ptr<AttributeReader> &reader,
                                                   const uint64_t offset, int64_t numToRead) {
            const auto poses = reader->readPositions(offset, numToRead);
            const auto attributes = reader->getAttributes();
            for (auto &pos: poses) {
                const auto idx = coordinate2Index(pos, attributes);
                ++grid[idx];
            }
        };

        PCLiteThreadPool pool(std::max(std::thread::hardware_concurrency(), static_cast<uint>(4)));
        auto batchSize = 1'000'000;
        for (const auto &reader: readers_) {
            auto bpp = reader->headerInfo().bpp_;
            auto numPoints = std::max(reader->headerInfo().numPoints_,
                                      reader->headerInfo().extendedNumPoints_);

            auto pointsLeft = numPoints;
            auto numRead = 0;

            while (pointsLeft > 0) {
                int64_t numToRead;
                if (pointsLeft < batchSize) {
                    numToRead = pointsLeft;
                    pointsLeft = 0;
                } else {
                    numToRead = batchSize;
                    pointsLeft = pointsLeft - batchSize;
                }

                pool.enqueue(countTask, reader, numRead, numToRead);
                numRead += batchSize;
            }
        }
        pool.waitAll();
        return grid;
    }

    Chunker::NodeLut createNodeLut(const std::vector<std::atomic_int32_t> &grid, uint gridSize) {
        auto iterateXYZ = [](int64_t gridSize, std::function<void(int64_t, int64_t, int64_t)> callback) {
            for (int x = 0; x < gridSize; x++) {
                for (int y = 0; y < gridSize; y++) {
                    for (int z = 0; z < gridSize; z++) {
                        callback(x, y, z);
                    }
                }
            }
        };

        std::vector<int64_t> grid_high;
        grid_high.reserve(grid.size());
        for (auto &value: grid) {
            grid_high.emplace_back(value);
        }

        auto level_max = int64_t(std::log2(gridSize));

        for (int64_t level_low = level_max - 1; level_low >= 0; level_low--) {
            int64_t level_high = level_low + 1;

            int64_t gridSize_high = pow(2, level_high);
            int64_t gridSize_low = pow(2, level_low);

            std::vector<int64_t> grid_low(gridSize_low * gridSize_low * gridSize_low, 0);
            // grid_high

            // loop through all cells of the lower detail target grid, and for each cell through the 8 enclosed cells of the higher level grid
            iterateXYZ(
                gridSize_low, [&grid_low, &grid_high, gridSize_low, gridSize_high, level_low, level_high, level_max]
        (int64_t x, int64_t y, int64_t z) {
                    int64_t index_low = x + y * gridSize_low + z * gridSize_low * gridSize_low;

                    int64_t sum = 0;
                    int64_t max = 0;
                    bool unmergeable = false;

                    // loop through the 8 enclosed cells of the higher detailed grid
                    for (int64_t j = 0; j < 8; j++) {
                        int64_t ox = (j & 0b100) >> 2;
                        int64_t oy = (j & 0b010) >> 1;
                        int64_t oz = (j & 0b001) >> 0;

                        int64_t nx = 2 * x + ox;
                        int64_t ny = 2 * y + oy;
                        int64_t nz = 2 * z + oz;

                        int64_t index_high = nx + ny * gridSize_high + nz * gridSize_high * gridSize_high;

                        auto value = grid_high[index_high];

                        if (value == -1) {
                            unmergeable = true;
                        } else {
                            sum += value;
                        }

                        max = std::max(max, value);
                    }


                    if (unmergeable || sum > maxPointsPerChunk) {
                        // finished chunks
                        for (int64_t j = 0; j < 8; j++) {
                            int64_t ox = (j & 0b100) >> 2;
                            int64_t oy = (j & 0b010) >> 1;
                            int64_t oz = (j & 0b001) >> 0;

                            int64_t nx = 2 * x + ox;
                            int64_t ny = 2 * y + oy;
                            int64_t nz = 2 * z + oz;

                            int64_t index_high = nx + ny * gridSize_high + nz * gridSize_high * gridSize_high;

                            auto value = grid_high[index_high];


                            if (value > 0) {
                                string nodeID = toNodeID(level_high, gridSize_high, nx, ny, nz);

                                Node node(nodeID, value);
                                node.x = nx;
                                node.y = ny;
                                node.z = nz;
                                node.size = pow(2, (level_max - level_high));

                                nodes.push_back(node);
                            }
                        }

                        // invalidate the field to show the parent that nothing can be merged with it
                        grid_low[index_low] = -1;
                    } else {
                        grid_low[index_low] = sum;
                    }
                });

            grid_high = grid_low;
        }

        // - create lookup table
        // - loop through nodes, add pointers to node/chunk for all enclosed cells in LUT.
        vector<int32_t> lut(gridSize * gridSize * gridSize, -1);
        for (int i = 0; i < nodes.size(); i++) {
            auto node = nodes[i];

            iterateXYZ(node.size, [node, &lut, gridSize, i](int64_t ox, int64_t oy, int64_t oz) {
                int64_t x = node.size * node.x + ox;
                int64_t y = node.size * node.y + oy;
                int64_t z = node.size * node.z + oz;
                int64_t index = x + y * gridSize + z * gridSize * gridSize;

                lut[index] = i;
            });
        }

        return {gridSize, lut};
    }

    static uint64_t coordinate2Index(const vec3d &pos, const Attributes &attributes) {
        // transfer las integer coordinates to new scale/offset/box values
        const auto x = pos.x;
        const auto y = pos.y;
        const auto z = pos.z;
        const auto posOffset = attributes.posOffset_;
        const auto posScale = attributes.posScale_;

        int32_t X = static_cast<int32_t>((x - posOffset.x) / posScale.x);
        int32_t Y = int32_t((y - posOffset.y) / posScale.y);
        int32_t Z = int32_t((z - posOffset.z) / posScale.z);

        double ux = (double(X) * posScale.x + posOffset.x - min.x) / size.x;
        double uy = (double(Y) * posScale.y + posOffset.y - min.y) / size.y;
        double uz = (double(Z) * posScale.z + posOffset.z - min.z) / size.z;

        bool inBox = ux >= 0.0 && uy >= 0.0 && uz >= 0.0;
        inBox = inBox && ux <= 1.0 && uy <= 1.0 && uz <= 1.0;

        uint64_t ix = uint64_t(std::min(dGridSize * ux, dGridSize - 1.0));
        uint64_t iy = uint64_t(std::min(dGridSize * uy, dGridSize - 1.0));
        uint64_t iz = uint64_t(std::min(dGridSize * uz, dGridSize - 1.0));

        uint64_t index = ix + iy * gridSize + iz * gridSize * gridSize;

        return index;
    }

    bool distributePoints(const Attributes &attributes) {
    }

private:
    friend class Chunker;
    Chunker *parent_;

    std::vector<std::unique_ptr<AttributeReader> > readers_;
    uint64_t maxPointsPerChunk_{0};
};

Chunker::Chunker(const std::vector<std::string> &sources, const std::string &target)
    : sources_(sources), target_(target) {
    d_ = std::make_unique<ChunkPrivate>(this);
}

Chunker::~Chunker() {
    d_ = nullptr;
}

bool Chunker::doChunking() {
    //1. calculate the grid size
    auto gridSize = d_->calculateGridSize();
    //2. count the point number in the cells
    auto grid = d_->countPointsInCells();
    //3. create a look up table
    auto nodeLut = d_->createNodeLut();
    //4. distribute points
    auto res = distributePoints();
    return res;
}
