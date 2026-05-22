#ifndef POINT_CLOUD_LOADER_H
#define POINT_CLOUD_LOADER_H

#include <memory>
#include <string>
#include <vector>
#include "point_layout.h"
#include "node.h"
#include "vec3.h"

class PointCloudLoader {
public:
    explicit PointCloudLoader(const std::string& dir);

    const PointLayout& layout()  const { return layout_; }
    vec3d bboxMin()              const { return bboxMin_; }
    vec3d bboxMax()              const { return bboxMax_; }
    const std::string& dir()     const { return dir_; }

    // Transfers ownership of the loaded nodes; call once.
    std::vector<std::unique_ptr<Node>> takeNodes() { return std::move(nodes_); }

private:
    std::string dir_;
    PointLayout layout_;
    vec3d       bboxMin_{}, bboxMax_{};
    std::vector<std::unique_ptr<Node>> nodes_;

    void parseHierarchy(const std::string& path, uint64_t firstChunkSize);
};

#endif //POINT_CLOUD_LOADER_H
