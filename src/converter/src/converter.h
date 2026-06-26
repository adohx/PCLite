//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_CONVERTER_H
#define PCLITE_CONVERTER_H

#include <functional>
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

// stage is one of "chunking"/"hierarchy"/"sampling"/"merging"/"rebuilding";
// fraction is 0 on a stage's entry, 1 on its exit, and (for "sampling" only
// -- the usual dominant cost) a real per-chunk fraction in between. Called
// from whichever thread run() executes on; callers that need it on a
// specific thread (e.g. a UI thread) must hop themselves.
using ConverterProgressCallback = std::function<void(const std::string& stage, float fraction)>;

// Top-level driver (3.2.2): prepares a shared Attributes layout from all
// sources, then runs Chunker -> HierarchyBuilder -> Indexer::indexChunk (per
// chunk) -> Indexer::mergeChunks, finally backfilling metadata.json's
// "hierarchy" fields with the values computed during merging.
class Converter {
public:
    using Options = ConverterOptions;

    Converter(std::vector<std::string> sources, std::string target, Options options = {});

    void setProgressCallback(ConverterProgressCallback cb);

    bool run();

private:
    void reportProgress(const std::string& stage, float fraction) const;

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

    // 4. Runs Indexer::mergeChunks() for the skeleton tree.
    bool doMerging();

    // 5. Runs Indexer::rebuildIndex(): assembles hierarchy.bin from per-chunk
    // header files and removes the temporary chunks/ directory.
    bool doRebuildIndex();

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

    ConverterProgressCallback progressCallback_;
};

#endif //PCLITE_CONVERTER_H
