// PCLite point cloud viewer — wires the viewer library together.
// Usage: pcl_viewer <dataset_dir>

#include <fstream>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#include "window/sdl_window.h"
#include "camera/orbit_camera.h"
#include "layer/point_cloud_layer.h"
#include "node_management/all_nodes_strategy.h"
#include "node_loader/file_node_loader.h"
#include "node_loader/point_layout.h"
#include "painter/node_painter.h"
#include "node.h"
#include "vec3.h"

// ─── Metadata / hierarchy parsing ────────────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
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

struct Metadata {
    vec3d    offset, scale, bboxMin, bboxMax;
    uint64_t firstChunkSize = 4004;
    PointLayout layout;
};

static Metadata parseMetadata(const std::string& path) {
    auto s = readFile(path);
    Metadata m;
    jsonVec3(s, "offset", m.offset);
    jsonVec3(s, "scale",  m.scale);

    auto bb = s.find("\"boundingBox\"");
    if (bb != std::string::npos) {
        auto mi = s.find("\"min\"", bb);
        auto ma = s.find("\"max\"", bb);
        jsonVec3(s.substr(mi), "min", m.bboxMin);
        jsonVec3(s.substr(ma), "max", m.bboxMax);
    }

    auto hier = s.find("\"hierarchy\"");
    if (hier != std::string::npos)
        m.firstChunkSize = jsonUint(s.substr(hier), "firstChunkSize", 4004);

    // Parse attributes array
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

        auto np = entry.find("\"name\"");
        std::string name;
        if (np != std::string::npos) {
            auto q1 = entry.find('"', np + 6) + 1;
            name = entry.substr(q1, entry.find('"', q1) - q1);
        }

        uint32_t sz = (uint32_t)jsonUint(entry, "size", 0);
        if (sz > 0) {
            m.layout.attrSizes.push_back(sz);
            if (name == "position") m.layout.posIdx = idx;
            if (name == "rgb")      m.layout.rgbIdx = idx;
            ++idx;
        }
        pos = cb + 1;
    }

    m.layout.scale  = m.scale;
    m.layout.offset = m.offset;
    return m;
}

static std::vector<std::unique_ptr<Node>> loadHierarchy(
    const std::string& path, uint64_t count,
    const Metadata& meta)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<std::unique_ptr<Node>> nodes;
    nodes.reserve(count);

    for (uint64_t i = 0; i < count; ++i) {
        uint8_t  type, mask;
        uint32_t nPts;
        uint64_t addr, bsz;
        f.read(reinterpret_cast<char*>(&type), 1);
        f.read(reinterpret_cast<char*>(&mask), 1);
        f.read(reinterpret_cast<char*>(&nPts), 4);
        f.read(reinterpret_cast<char*>(&addr), 8);
        f.read(reinterpret_cast<char*>(&bsz),  8);

        nodes.push_back(std::make_unique<Node>(
            static_cast<NodeType>(type), mask, nPts, addr, bsz,
            meta.bboxMin, meta.bboxMax));
    }
    return nodes;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pcl_viewer <dataset_dir>\n");
        return 1;
    }
    std::string dir = argv[1];
    if (dir.back() == '/') dir.pop_back();

    // 1. Parse metadata
    auto meta = parseMetadata(dir + "/metadata.json");
    printf("BBox: [%.2f %.2f %.2f] - [%.2f %.2f %.2f]\n",
        meta.bboxMin.x, meta.bboxMin.y, meta.bboxMin.z,
        meta.bboxMax.x, meta.bboxMax.y, meta.bboxMax.z);

    // 2. Load hierarchy nodes
    auto nodes = loadHierarchy(dir + "/hierarchy.bin", meta.firstChunkSize, meta);
    printf("Hierarchy: %zu nodes\n", nodes.size());

    // Cap to a point budget
    const uint64_t POINT_BUDGET = 2'000'000;
    uint64_t accumulated = 0;
    size_t   keepCount   = 0;
    for (auto& n : nodes) {
        if (n->type() == NodeType::Proxy) { ++keepCount; continue; }
        accumulated += n->pointCount();
        ++keepCount;
        if (accumulated >= POINT_BUDGET) break;
    }
    nodes.resize(keepCount);
    printf("Loading %zu nodes (~%llu pts)\n", keepCount, (unsigned long long)accumulated);

    // 3. Build layer
    auto layer = std::make_unique<PointCloudLayer>();

    auto loader   = std::make_unique<FileNodeLoader>(dir + "/octree.bin");
    auto strategy = std::make_unique<AllNodesStrategy>();
    strategy->setNodeLoader(loader.get());

    auto painter = std::make_unique<NodePainter>(meta.layout);

    auto& mgr = layer->nodeManager();
    mgr.addStrategy(std::move(strategy));
    mgr.addPainter(std::move(painter));
    for (auto& n : nodes)
        mgr.addNode(std::move(n));

    // 4. Build camera
    vec3d center = {
        (meta.bboxMin.x + meta.bboxMax.x) * 0.5,
        (meta.bboxMin.y + meta.bboxMax.y) * 0.5,
        (meta.bboxMin.z + meta.bboxMax.z) * 0.5,
    };
    float span = (float)std::max({
        meta.bboxMax.x - meta.bboxMin.x,
        meta.bboxMax.y - meta.bboxMin.y,
        meta.bboxMax.z - meta.bboxMin.z});

    auto camera = std::make_unique<OrbitCamera>();
    camera->setTarget(center);
    camera->setFarPlane(span * 10.f);
    camera->setNearPlane(span * 0.001f);

    // 5. Build window and run
    SDLWindow window(1280, 800, "PCLite Viewer");
    window.setOrbitDistance(span * 1.2f);
    window.addCamera(std::move(camera));
    window.addLayer(std::move(layer));

    // loader must outlive the window (strategy holds a raw pointer to it)
    window.run();

    return 0;
}
