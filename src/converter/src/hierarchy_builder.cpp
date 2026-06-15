//
// Created by cj on 2026-06-14.
//

#include "hierarchy_builder.h"

#include "octree_naming.h"

HierarchyBuilder::HierarchyBuilder(BoundingBoxd rootAABB) : rootAABB_(rootAABB) {}

std::shared_ptr<Node> HierarchyBuilder::getOrCreateChild(const std::shared_ptr<Node> &parent, int childIndex) {
    if (parent->children_[childIndex]) return parent->children_[childIndex];

    BoundingBoxd childBB = octree_naming::childBoundingBox(parent->bb_, childIndex);
    auto child = std::make_shared<Node>(parent->name_ + std::to_string(childIndex), childBB, parent);
    child->level_ = parent->level_ + 1;

    parent->children_[childIndex] = child;
    return child;
}

std::shared_ptr<Node> HierarchyBuilder::build(const std::vector<ChunkInfo> &chunks,
                                               std::vector<std::shared_ptr<Node>> &outChunkRoots) {
    auto root = std::make_shared<Node>("r", rootAABB_);
    root->level_ = 0;

    outChunkRoots.clear();
    outChunkRoots.reserve(chunks.size());

    for (const ChunkInfo &chunk : chunks) {
        std::shared_ptr<Node> current = root;
        for (size_t i = 1; i < chunk.name.size(); ++i) {
            int childIndex = chunk.name[i] - '0';
            current = getOrCreateChild(current, childIndex);
        }

        current->numPoints_ = static_cast<uint32_t>(chunk.numPoints);
        outChunkRoots.push_back(current);
    }

    return root;
}
