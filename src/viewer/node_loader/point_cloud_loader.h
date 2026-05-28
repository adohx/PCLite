#ifndef POINT_CLOUD_LOADER_H
#define POINT_CLOUD_LOADER_H

#include "node_loader.h"
#include "attributes.h"
#include "bounding_box.h"
#include "vec3.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

class PointCloudLoader : public NodeLoader<Node> {
public:
    explicit PointCloudLoader(const std::string& dir);

    ~PointCloudLoader() override;

    const Attributes&  attributes() const { return attributes_; }
    BoundingBoxd boundingBox() const { return header_.bbox_; }
    const std::string& dir()        const { return dir_; }

    bool load(std::shared_ptr<Node> node)                override;
    std::shared_ptr<Node> loadRoot()                     override;

private:
    bool loadHierarchyChunk(std::shared_ptr<Node> node);
    bool loadPoints(std::shared_ptr<Node> node);
    void parseMetadata(const std::string& path);
    BoundingBoxd createChildAABB(const BoundingBoxd& parentBB, int childIdx);

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
