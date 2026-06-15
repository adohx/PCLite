//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_CONVERTER_H
#define PCLITE_CONVERTER_H

#include <memory>
#include <string>
#include <vector>

#include "attributes.h"
#include "chunker.h"
#include "converter_options.h"
#include "hierarchy_builder.h"
#include "indexer.h"
#include "node.h"

class AttributeReader;
class ConcurrentWriter;

// Top-level driver (3.2.2): prepares a shared Attributes layout from all
// sources, then runs Chunker -> HierarchyBuilder -> Indexer::indexChunk (per
// chunk) -> Indexer::mergeChunks, finally backfilling metadata.json's
// "hierarchy" fields with the values computed during merging.
class Converter {
public:
    using Options = ConverterOptions;

    Converter(std::vector<std::string> sources, std::string target, Options options = {});

    bool run();

private:
    // Creates an AttributeReader for every source and adopts the first
    // reader's Attributes as the unified row layout (3.2.2 step 0). Per the
    // simplified design, all sources must report the same attribute name set;
    // otherwise this fails.
    bool prepareAttributes();

    // 1. Runs Chunker::run(): computes the AABB, distributes every source's
    // points into "chunks/<name>.bin", and writes the initial metadata.json.
    bool doChunking();

    // 2. Runs HierarchyBuilder::build() to create the skeleton tree above the
    // chunk-root nodes.
    bool buildHierarchy();

    // 3. Runs Indexer::indexChunk() for every chunk (in parallel).
    bool doSampling();

    // 4. Runs Indexer::mergeChunks() for the skeleton tree, then rewrites
    // metadata.json's "hierarchy.depth"/"hierarchy.firstChunkSize" fields.
    bool doMerging();

    // Rewrites metadata.json's "hierarchy" object with the depth and
    // hierarchy.bin byte size computed by Indexer::mergeChunks().
    bool updateMetadataHierarchy(int depth, uint64_t hierarchyByteSize) const;

private:
    std::vector<std::string> sources_;
    std::string target_;
    Options options_;

    std::vector<std::shared_ptr<AttributeReader>> readers_;
    Attributes attributes_;
    std::shared_ptr<ConcurrentWriter> writer_;

    std::vector<ChunkInfo> chunks_;
    std::vector<std::shared_ptr<Node>> chunkRoots_;
    std::shared_ptr<Node> hierarchyRoot_;

    std::unique_ptr<Chunker> chunker_;
    std::unique_ptr<HierarchyBuilder> hierarchyBuilder_;
    std::unique_ptr<Indexer> indexer_;
};

#endif //PCLITE_CONVERTER_H
