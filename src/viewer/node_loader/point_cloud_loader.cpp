#include "point_cloud_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

// ─── Minimal JSON helpers ─────────────────────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static double jsonDouble(const std::string& s, const std::string& key, double def = 0.0) {
    auto p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return def;
    p = s.find_first_of("-0123456789", p + key.size() + 2);
    return p == std::string::npos ? def : std::stod(s.substr(p));
}

static uint64_t jsonUint(const std::string& s, const std::string& key, uint64_t def = 0) {
    auto p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return def;
    p = s.find_first_of("0123456789", p + key.size() + 2);
    return p == std::string::npos ? def : std::stoull(s.substr(p));
}

static void jsonVec3(const std::string& s, const std::string& key, vec3d& out) {
    auto p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return;
    p = s.find('[', p);
    if (p == std::string::npos) return;
    auto q = s.find_first_of("-0123456789", ++p);
    out.x = std::stod(s.substr(q));
    q = s.find_first_of("-0123456789", s.find(',', q) + 1);
    out.y = std::stod(s.substr(q));
    q = s.find_first_of("-0123456789", s.find(',', q) + 1);
    out.z = std::stod(s.substr(q));
}

// ─── PointCloudLoader ────────────────────────────────────────────────────────────

PointCloudLoader::PointCloudLoader(const std::string& dir) : dir_(dir) {
    uint64_t firstChunkSize = 4004;
    parseMetadata(dir_ + "/metadata.json");

    // firstChunkSize is set inside parseMetadata; re-read so hierarchy can use it
    auto s = readFile(dir_ + "/metadata.json");
    auto hier = s.find("\"hierarchy\"");
    if (hier != std::string::npos)
        firstChunkSize = jsonUint(s.substr(hier), "firstChunkSize", 4004);

    parseHierarchy(dir_ + "/hierarchy.bin", firstChunkSize);
}

void PointCloudLoader::parseMetadata(const std::string& path) {
    auto s = readFile(path);

    vec3d offset{}, scale{1,1,1};
    jsonVec3(s, "offset", offset);
    jsonVec3(s, "scale",  scale);

    auto bb = s.find("\"boundingBox\"");
    if (bb != std::string::npos) {
        auto mi = s.find("\"min\"", bb);
        auto ma = s.find("\"max\"", bb);
        jsonVec3(s.substr(mi), "min", bboxMin_);
        jsonVec3(s.substr(ma), "max", bboxMax_);
    }

    // Parse attributes array to build PointLayout
    auto arrStart = s.find('[', s.find("\"attributes\""));
    auto arrEnd   = s.rfind(']');
    std::string attrStr = s.substr(arrStart, arrEnd - arrStart + 1);

    int idx = 0;
    size_t pos = 0;
    while (true) {
        auto ob = attrStr.find('{', pos);
        if (ob == std::string::npos) break;
        auto cb = attrStr.find('}', ob);
        auto entry = attrStr.substr(ob, cb - ob + 1);

        std::string name;
        auto np = entry.find("\"name\"");
        if (np != std::string::npos) {
            auto q1 = entry.find('"', np + 6) + 1;
            name = entry.substr(q1, entry.find('"', q1) - q1);
        }

        auto sz = (uint32_t)jsonUint(entry, "size", 0);
        if (sz > 0) {
            layout_.attrSizes.push_back(sz);
            if (name == "position") layout_.posIdx = idx;
            if (name == "rgb")      layout_.rgbIdx = idx;
            ++idx;
        }
        pos = cb + 1;
    }

    layout_.scale  = scale;
    layout_.offset = offset;
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
