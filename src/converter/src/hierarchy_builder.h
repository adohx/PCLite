//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_HIERARCHY_BUILDER_H
#define PCLITE_HIERARCHY_BUILDER_H

#include <memory>
#include <vector>

#include "bounding_box.h"
#include "chunker.h"
#include "node.h"

// Builds the skeleton of the global hierarchy tree (3.2.7): for every
// ChunkInfo, walks from the root down to the node identified by its octree
// path (ChunkInfo::name, e.g. "r047"), creating any missing intermediate
// nodes along the way with bb_/level_/parent_/children_ set. Nodes above the
// chunk roots (closer to "r") are fully created here; the chunk-root nodes
// and everything below them are filled in later by Indexer.
class HierarchyBuilder {
public:
    explicit HierarchyBuilder(BoundingBoxd rootAABB);

    // Returns the root node ("r"). outChunkRoots is filled 1:1 with `chunks`,
    // outChunkRoots[i] being the (possibly newly created) node at the octree
    // path chunks[i].name, with numPoints_ set to chunks[i].numPoints.
    std::shared_ptr<Node> build(const std::vector<ChunkInfo> &chunks,
                                 std::vector<std::shared_ptr<Node>> &outChunkRoots);

private:
    static std::shared_ptr<Node> getOrCreateChild(const std::shared_ptr<Node> &parent, int childIndex);

private:
    BoundingBoxd rootAABB_;
};

#endif //PCLITE_HIERARCHY_BUILDER_H
