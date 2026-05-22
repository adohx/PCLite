#include "point_cloud_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

// ─── static helpers ───────────────────────────────────────────────────────────

static vec3d vec3dFrom(const json& arr) {
    return { arr[0].get<double>(), arr[1].get<double>(), arr[2].get<double>() };
}

static void applyMetadata(const json& meta,
                          PointLayout& layout,
                          vec3d& bboxMin, vec3d& bboxMax)
{
    layout.offset = vec3dFrom(meta["offset"]);
    layout.scale  = vec3dFrom(meta["scale"]);

    bboxMin = vec3dFrom(meta["boundingBox"]["min"]);
    bboxMax = vec3dFrom(meta["boundingBox"]["max"]);

    int idx = 0;
    for (const auto& attr : meta["attributes"]) {
        auto size = attr["size"].get<uint32_t>();
        if (size == 0) continue;

        layout.attrSizes.push_back(size);

        auto name = attr["name"].get<std::string>();
        if (name == "position") layout.posIdx = idx;
        if (name == "rgb")      layout.rgbIdx = idx;
        ++idx;
    }
}

// ─── PointCloudLoader ─────────────────────────────────────────────────────────

PointCloudLoader::PointCloudLoader(const std::string& dir) : dir_(dir) {
    std::ifstream f(dir_ + "/metadata.json");
    if (!f) throw std::runtime_error("Cannot open: " + dir_ + "/metadata.json");

    const json meta = json::parse(f);
    applyMetadata(meta, layout_, bboxMin_, bboxMax_);
    parseHierarchy(dir_ + "/hierarchy.bin",
                   meta["hierarchy"]["firstChunkSize"].get<uint64_t>());
}

void PointCloudLoader::parseHierarchy(const std::string& path, uint64_t count) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);

    nodes_.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t  type, mask;
        uint32_t nPts;
        uint64_t addr, bsz;
        f.read(reinterpret_cast<char*>(&type), 1);
        f.read(reinterpret_cast<char*>(&mask), 1);
        f.read(reinterpret_cast<char*>(&nPts), 4);
        f.read(reinterpret_cast<char*>(&addr), 8);
        f.read(reinterpret_cast<char*>(&bsz),  8);

        nodes_.push_back(std::make_unique<Node>(
            static_cast<NodeType>(type), mask, nPts, addr, bsz,
            bboxMin_, bboxMax_));
    }
}
