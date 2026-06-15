//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_CHUNK_H
#define PCLITE_CHUNK_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "attributes.h"
#include "bounding_box.h"
#include "converter_options.h"

class AttributeReader;
class ConcurrentWriter;

// One leaf cell of the coarse pre-chunking grid (see convert流程.md 3.2.6):
// `name` is its octree node id (e.g. "r", "r03"), `level`/`x`/`y`/`z`/`size`
// locate it within the grid at `level` (a `size`^3 region starting at
// (x,y,z)*size), and `bb` is its world-space bounding box (a sub-cube of the
// root AABB).
struct ChunkInfo {
    std::string name;
    int level = 0;
    uint32_t x = 0, y = 0, z = 0;
    uint32_t size = 0;
    uint64_t numPoints = 0;
    BoundingBoxd bb;
};

// Pre-chunking stage (3.2.6/3.3.2): reads all sources once to build a coarse
// grid of point counts, merges cells bottom-up into ChunkInfo entries small
// enough to be indexed independently, then re-reads all sources to re-encode
// every point into `attributes_`'s layout and distribute the encoded rows
// into per-chunk files under "chunks/<name>.bin", while accumulating global
// per-attribute stats (min_/max_/histogram_).
class Chunker {
public:
    Chunker(std::vector<std::shared_ptr<AttributeReader>> readers,
            Attributes attributes,
            std::string targetDir,
            std::shared_ptr<ConcurrentWriter> writer,
            ConverterOptions options);

    // Runs computeAABB -> calculateGridSize -> countPointsInCells ->
    // buildChunksAndLut -> distributePoints -> writeMetadata.
    bool run();

    const std::vector<ChunkInfo> &chunks() const { return chunks_; }
    const BoundingBoxd &aabb() const { return aabb_; }
    const Attributes &attributes() const { return attributes_; }
    uint32_t gridSize() const { return gridSize_; }

private:
    // Cube bounding box covering all sources (min = combined min, side =
    // max extent along any axis).
    BoundingBoxd computeAABB() const;

    // Power-of-two finest-grid resolution, based on total point count and
    // options_.minGridSize.
    uint32_t calculateGridSize(uint64_t totalPoints) const;

    // Point counts for each of the gridSize^3 cells of aabb_, built by
    // streaming readPositions() from every reader.
    std::vector<int64_t> countPointsInCells(uint32_t gridSize) const;

    // Bottom-up merge of `grid` (gridSize^3 finest-level counts) into chunks
    // small enough that no chunk exceeds maxPointsPerChunk (a chunk that
    // can't merge further because a descendant cell was already finalized is
    // also kept as-is). Fills `outChunks` and `outLut`, a gridSize^3 lookup
    // table mapping each finest-grid cell to its chunk's index in outChunks.
    void buildChunksAndLut(const std::vector<int64_t> &grid, uint32_t gridSize,
                           uint64_t maxPointsPerChunk,
                           std::vector<ChunkInfo> &outChunks,
                           std::vector<int32_t> &outLut) const;

    // Streams readRawData() from every reader, re-encodes each row into
    // attributes_'s layout via AttributeHandlerRegistry, looks up its chunk
    // via `lut`, appends it to "chunks/<chunk.name>.bin", and folds it into
    // attributes_[*].min_/max_/histogram_ via updateStats. Also fills
    // chunks_[*].numPoints.
    bool distributePoints(const std::vector<int32_t> &lut, uint32_t gridSize);

    // Writes targetDir/metadata.json (2.1 layout). hierarchy.{depth,firstChunkSize}
    // are written as placeholders (0 / options_.firstChunkSize) and are
    // expected to be overwritten later by the merge stage.
    bool writeMetadata() const;

private:
    std::vector<std::shared_ptr<AttributeReader>> readers_;
    Attributes attributes_;
    std::string targetDir_;
    std::shared_ptr<ConcurrentWriter> writer_;
    ConverterOptions options_;

    BoundingBoxd aabb_;
    uint32_t gridSize_ = 0;
    std::vector<ChunkInfo> chunks_;
};

#endif //PCLITE_CHUNK_H
