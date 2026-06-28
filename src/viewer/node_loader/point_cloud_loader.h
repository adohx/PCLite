#ifndef POINT_CLOUD_LOADER_H
#define POINT_CLOUD_LOADER_H

#include "node_loader.h"
#include "attributes.h"
#include "bounding_box.h"
#include "vec3.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class PointCloudLoader : public NodeLoader<Node> {
public:
    explicit PointCloudLoader(const std::string& dir);

    ~PointCloudLoader() override;

    const Attributes&  attributes() const { return attributes_; }
    BoundingBoxd boundingBox() const { return header_.bbox_; }
    const std::string& dir()        const { return dir_; }
    // Total point count of the full dataset (from metadata.json), regardless
    // of how much is currently loaded/rendered at the active LOD.
    uint64_t totalPoints() const { return header_.points_; }

    bool load(std::shared_ptr<Node> node)                override;
    std::shared_ptr<Node> loadRoot()                     override;

    // Result of queryFinestLeafAt: the full-resolution points and KD-tree
    // for the finest octree leaf covering a query point, decoded into
    // fresh, caller-owned storage -- no live Node is read or written.
    struct LeafQueryResult {
        bool found = false;
        std::vector<vec3f> points;
        KDTree3 kdTree;
    };

    // Thread-safe and side-effect-free with respect to the live Node tree
    // (guarded only by fileMutex_, touches no shared Node object): replays
    // the hierarchy descent toward `point` in local scratch structures,
    // landing on the finest available leaf, then reads its point + KD-tree
    // data straight off disk into the returned result. Safe to call from a
    // background thread concurrently with the main thread's ordinary
    // LRU-driven Node loading, since the two never touch the same objects.
    // Used by the viewer's pick-assist refinement so it always gets
    // full-resolution data regardless of which LOD is currently rendered.
    LeafQueryResult queryFinestLeafAt(vec3d point);

private:
    bool loadHierarchyChunkLocked(const std::shared_ptr<Node>& node);
    bool loadPointsLocked(const std::shared_ptr<Node>& node);
    void loadKdtreeBytesLocked(const std::shared_ptr<Node>& node);
    void parseMetadata(const std::string& path);
    void parseKdtreeIndex(const std::string& path);
    BoundingBoxd createChildAABB(const BoundingBoxd& parentBB, int childIdx);

    // Local, non-shared mirror of one hierarchy batch's records -- same
    // BFS layout as loadHierarchyChunkLocked builds into the live Node
    // tree, but kept entirely separate so queryFinestLeafAt never touches
    // anything the main thread can see.
    struct ScratchNode {
        std::string name;
        BoundingBoxd bb;
        NodeType type = NodeType::Normal;
        uint64_t address = 0, byteSize = 0;
        uint64_t proxyChunkAddr = 0, proxyChunkSize = 0;
        int children[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    };
    std::vector<ScratchNode> parseHierarchyBatchLocked(uint64_t addr, uint64_t size,
                                                        const std::string& rootName,
                                                        const BoundingBoxd& rootBB);

    template<typename T>
    T readLE(const char* data, size_t offset) const {
        T v{};
        memcpy(&v, data + offset, sizeof(T));
        return v;
    }

    std::string   dir_;
    Attributes    attributes_;
    std::ifstream octreeFile_;
    std::ifstream hierarchyFile_;
    std::ifstream kdtreeFile_;       // may be !is_open() for datasets converted before kdtree.bin existed
    mutable std::mutex fileMutex_;   // guards octreeFile_/hierarchyFile_/kdtreeFile_ across threads

    struct KdtreeLocation { uint64_t offset; uint64_t size; };
    std::unordered_map<std::string, KdtreeLocation> kdtreeIndex_;

    struct Header {
        std::string version_;
        std::string name_;
        std::string description_;
        uint64_t points_;
        std::string projection_;

        double spacing_;
        BoundingBoxd bbox_;
        std::string encoding_;
        struct Hierarchy {
            int64_t chunkSize_;
            int64_t stepSize_;
            int64_t depth_;
        } firstChunk_;
    } header_;
    const uint32_t BYTES_PER_HIERARCHY_NODE=22;
};

#endif //POINT_CLOUD_LOADER_H
