#ifndef POINT_CLOUD_LOADER_H
#define POINT_CLOUD_LOADER_H

#include "node_loader.h"
#include "attributes.h"
#include "vec3.h"
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "bounding_box.h"

class PointCloudLoader : public NodeLoader {
public:
    explicit PointCloudLoader(const std::string& dir);

    const Attributes&  attributes() const { return attributes_; }
    vec3d              bboxMin()    const { return bboxMin_; }
    vec3d              bboxMax()    const { return bboxMax_; }
    const std::string& dir()        const { return dir_; }

    void setTarget(Node& node) override;
    void load()                override;
private:
    bool loadHierarchy();

    BoundingBoxd createChildAABB(const BoundingBoxd &parentBB, int childIdx);

    bool loadPoints();
    void parseMetadata(const std::string& path);

    template<typename T>
    T readLittleEndian(const char* data, size_t offset) {
        T value = 0;
        memcpy(&value, data + offset, sizeof(T));
        return value;
    }

private:
    std::string   dir_;
    const std::string octreeFile_ = "octree.bin";
    const std::string metadataFile_ = "metadata.json";
    const std::string hierarchyFile_ = "hierarchy.bin";

    Attributes    attributes_;
    vec3d         bboxMin_{}, bboxMax_{};
    uint64_t      firstChunkSize_ = 4004;
    std::ifstream octreeFile_;


};

#endif //POINT_CLOUD_LOADER_H
