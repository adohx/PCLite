#include "point_cloud_loader.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

static vec3d vec3dFrom(const json &a) {
    return {a[0].get<double>(), a[1].get<double>(), a[2].get<double>()};
}

static AttributeType parseType(const std::string &t) {
    if (t == "int8") return AttributeType::INT8;
    if (t == "int16") return AttributeType::INT16;
    if (t == "int32") return AttributeType::INT32;
    if (t == "int64") return AttributeType::INT64;
    if (t == "uint8") return AttributeType::UINT8;
    if (t == "uint16") return AttributeType::UINT16;
    if (t == "uint32") return AttributeType::UINT32;
    if (t == "uint64") return AttributeType::UINT64;
    if (t == "float") return AttributeType::FLOAT;
    if (t == "double") return AttributeType::DOUBLE;
    return AttributeType::UNDEFINED;
}

// ─── Construction ─────────────────────────────────────────────────────────────

PointCloudLoader::PointCloudLoader(const std::string &dir) : dir_(dir) {
    parseMetadata(dir_ + "/metadata.json");
    octreeFile_.open(dir_ + "/octree.bin", std::ios::in|std::ios::binary);
    if (!octreeFile_.is_open())
        throw std::runtime_error("Cannot open: " + dir_ + "/octree.bin");

    hierarchyFile_.open(dir_ + "/hierarchy.bin", std::ios::in|std::ios::binary);
    if (!hierarchyFile_.is_open())
        throw std::runtime_error("Cannot open: " + dir_ + "/hierarchy.bin");
}

PointCloudLoader::~PointCloudLoader() {
    octreeFile_.close();
    hierarchyFile_.close();
}

void PointCloudLoader::parseMetadata(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    const json meta = json::parse(f);

    auto min = vec3dFrom(meta["boundingBox"]["min"]);
    auto max = vec3dFrom(meta["boundingBox"]["max"]);

    header_.version_ = meta["version"].get<std::string>();
    header_.name_ = meta["name"].get<std::string>();
    header_.description_ = meta["description"].get<std::string>();
    header_.points_ = meta["points"].get<uint64_t>();
    header_.projection_ = meta["projection"].get<std::string>();
    header_.spacing_ = meta["spacing"].get<double>();
    header_.bbox_ = BoundingBoxd(min, max);
    header_.encoding_ = meta["encoding"].get<std::string>();

    header_.firstChunk_.chunkSize_ = meta["hierarchy"]["firstChunkSize"].get<uint64_t>();
    header_.firstChunk_.stepSize_ = meta["hierarchy"]["stepSize"].get<uint64_t>();
    header_.firstChunk_.depth_ = meta["hierarchy"]["depth"].get<uint64_t>();

    for (const auto &a: meta["attributes"]) {
        Attribute attr;
        attr.name_ = a["name"].get<std::string>();
        attr.description_ = a["description"].get<std::string>();
        attr.bytes_ = a["size"].get<int>();
        attr.numElements_ = a["numElements"].get<int>();
        attr.type_ = parseType(a["type"].get<std::string>());

        auto mn = a["min"], mx = a["max"];
        auto scale = a["scale"], offset = a["offset"];

        attr.min_ = (mn.size() >= 3) ? vec3dFrom(mn) : vec3d{mn[0].get<double>(), 0, 0};
        attr.max_ = (mx.size() >= 3) ? vec3dFrom(mx) : vec3d{mx[0].get<double>(), 0, 0};
        attr.offset_ = (offset.size() >= 3) ? vec3dFrom(offset) : vec3d{0, 0, 0};
        attr.scale_ = (scale.size() >= 3) ? vec3dFrom(scale) : vec3d{1, 1, 1};

        attributes_.pushAttribute(attr);
    }
}

BoundingBoxd PointCloudLoader::createChildAABB(const BoundingBoxd &bb, int idx) {
    auto mn = bb.min();
    auto mx = bb.max();
    auto size = bb.getSize();

    if (idx & 1) mn.z += size.z / 2;
    else mx.z -= size.z / 2;
    if (idx & 2) mn.y += size.y / 2;
    else mx.y -= size.y / 2;
    if (idx & 4) mn.x += size.x / 2;
    else mx.x -= size.x / 2;

    return BoundingBoxd(mn, mx);
}

std::shared_ptr<Node> PointCloudLoader::loadRoot() {
    if (header_.firstChunk_.chunkSize_ % BYTES_PER_HIERARCHY_NODE != 0)
        return nullptr;

    auto root = std::make_shared<Node>("r", BoundingBoxd(header_.bbox_));
    root->level_ = 0;
    root->type_ = NodeType::Proxy;
    root->proxyChunkAddr_ = 0;
    root->proxyChunkSize_ = header_.firstChunk_.chunkSize_;

    loadHierarchyChunk(root);
    loadPoints(root);

    return root;
}

bool PointCloudLoader::loadHierarchyChunk(std::shared_ptr<Node> node) {
    if (!node)
        return false;

    auto hAddr = node->proxyChunkAddr_;
    auto hSize = node->proxyChunkSize_;
    if (hSize == 0) return false;

    std::vector<char> buffer(hSize);

    hierarchyFile_.seekg(static_cast<std::streamoff>(hAddr));
    if (!hierarchyFile_.read(buffer.data(), static_cast<std::streamsize>(hSize))) {
        std::cerr<<"读取header失败"<<std::endl;
    }

    int numNodes = (int) (hSize / BYTES_PER_HIERARCHY_NODE);

    std::vector<Node *> nodeList(numNodes, nullptr);
    nodeList[0] = node.get();

    int nodePos = 1;
    for (int i = 0; i < numNodes; ++i) {
        Node *cur = nodeList[i];
        if (!cur) break;

        const char *nd = buffer.data() + i * BYTES_PER_HIERARCHY_NODE;
        auto type = readLE<uint8_t>(nd, 0);
        auto childMask = readLE<uint8_t>(nd, 1);
        auto numPoints = readLE<uint32_t>(nd, 2);
        auto addr = readLE<uint64_t>(nd, 6);
        auto size = readLE<uint64_t>(nd, 14);

        if (cur->type_ == NodeType::Proxy) {
            cur->address_ = addr;
            cur->byteSize_ = size;
        } else if (static_cast<NodeType>(type) == NodeType::Proxy) {
            cur->proxyChunkAddr_ = addr;
            cur->proxyChunkSize_ = size;
        } else {
            cur->address_ = addr;
            cur->byteSize_ = size;
        }
        cur->type_ = static_cast<NodeType>(type);
        cur->numPoints_ = numPoints;
        if (cur->byteSize_ == 0) cur->numPoints_ = 0;
        if (cur->type_ == NodeType::Proxy) continue;

        for (int ci = 0; ci < 8; ++ci) {
            if (!((1 << ci) & childMask)) continue;
            auto childName = cur->name_ + std::to_string(ci);
            auto childBB = createChildAABB(cur->bb_, ci);
            auto child = std::make_shared<Node>(childName, childBB,
                                                std::shared_ptr<Node>(cur, [](Node *) {
                                                }));
            child->spacing_ = cur->spacing_ / 2.0;
            child->level_ = cur->level_ + 1;
            cur->children_[ci] = child;
            if (nodePos < numNodes)
                nodeList[nodePos++] = child.get();
        }
    }
    return true;
}

bool PointCloudLoader::loadPoints(std::shared_ptr<Node> node) {
    try {
        if (node->type_ == NodeType::Proxy) {
           return false;
        }

        auto buffer = std::vector<uint8_t>(node->byteSize_);
        octreeFile_.seekg(node->address_);
        if (!octreeFile_.read(reinterpret_cast<char*>(buffer.data()), node->byteSize_)){
            auto failed=true;
            std::cerr << "读取文件失败: " << std::strerror(errno) << std::endl;
        }

        node->setData(std::move(buffer), attributes_);
        node->setLoaded(true);
        node->setLoading(false);
    }
    catch (const std::exception& e) {
        node->setLoaded(false);
        node->setLoading(false);
        return false;
    }
    return true;
}


bool PointCloudLoader::load(std::shared_ptr<Node> node) {
    if (node->isLoaded() || node->isLoading()) return true;

    node->setLoading(true);

    if (node->type_ == NodeType::Proxy) {
        loadHierarchyChunk(node);
        return true;
    }

    return loadPoints(node);
}
