#include "point_cloud_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

#include "../../../cmake-build-debug/_deps/spdlog-src/include/spdlog/fmt/bundled/base.h"

using json = nlohmann::json;

static vec3d vec3dFrom(const json &arr) {
    return {arr[0].get<double>(), arr[1].get<double>(), arr[2].get<double>()};
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

// ─── PointCloudLoader ─────────────────────────────────────────────────────────

PointCloudLoader::PointCloudLoader(const std::string &dir) : dir_(dir) {
    parseMetadata(dir_ + "/metadata.json");
    octreeFile_.open(dir_ + "/octree.bin", std::ios::binary);
    if (!octreeFile_)
        throw std::runtime_error("Cannot open: " + dir_ + "/octree.bin");
}

void PointCloudLoader::parseMetadata(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    const json meta = json::parse(f);

    attributes_.posOffset_ = vec3dFrom(meta["offset"]);
    attributes_.posScale_ = vec3dFrom(meta["scale"]);

    bboxMin_ = vec3dFrom(meta["boundingBox"]["min"]);
    bboxMax_ = vec3dFrom(meta["boundingBox"]["max"]);

    firstChunkSize_ = meta["hierarchy"]["firstChunkSize"].get<uint64_t>();

    attributes_.bytes_ = 0;
    for (const auto &a: meta["attributes"]) {
        Attribute attr;
        attr.name_ = a["name"].get<std::string>();
        attr.description_ = a["description"].get<std::string>();
        attr.size_ = a["size"].get<int>();
        attr.numElements_ = a["numElements"].get<int>();
        attr.type_ = parseType(a["type"].get<std::string>());

        auto mn = a["min"];
        auto mx = a["max"];
        if (mn.size() >= 3) {
            attr.min_ = vec3dFrom(mn);
            attr.max_ = vec3dFrom(mx);
        } else {
            attr.min_ = {mn[0].get<double>(), 0, 0};
            attr.max_ = {mx[0].get<double>(), 0, 0};
        }

        attributes_.bytes_ += attr.size_;
        attributes_.attr_.push_back(std::move(attr));
    }
}

bool PointCloudLoader::loadHierarchy() {
    if (target_ == nullptr)
        return false;
    auto hAddr = target_->address;
    auto hSize = target_->byteSize;
    auto buffer = std::vector<char>(hSize);

    std::filesystem::path folder(dir_);
    auto hierarchyPath = folder / hierarchyFile_;

    std::ifstream f(hierarchyPath, std::ios::binary);

    if (!f)
        throw std::runtime_error("Cannot open: " + hierarchyPath.string());

    f.seekg(static_cast<std::streamoff>(hAddr));
    f.read(buffer.data(), static_cast<std::streamsize>(hSize));

    auto bytesPerNode = 22;
    auto numNodes = hSize / bytesPerNode;
    auto nodes = std::vector<Node>(numNodes);
    nodes[0] = target_;

    auto nodePos = 1;
    for (int i = 1; i < numNodes; i++) {
        auto &node = nodes[i];
        auto nodeData = buffer.data() + i * bytesPerNode;
        auto type = *reinterpret_cast<uint8_t *>(nodeData + 0);
        auto childMask = *reinterpret_cast<uint8_t *>(nodeData + 1);
        auto numPoints = *reinterpret_cast<uint32_t *>(nodeData + 2);
        auto addr = *reinterpret_cast<uint64_t *>(nodeData + 6);
        auto size = *reinterpret_cast<uint64_t *>(nodeData + 14);

        if (node.type == NodeType::Proxy) {
            node.address_ = addr;
            node.byteSize_ = size;
        } else if (type == NodeType::Proxy) {
            node.proxyChunkAddr_ = addr;
            node.proxyChunkSize_ = size;
        } else {
            node.address_ = addr;
            node.byteSize_ = size;
        }
        node.type = type;
        node.numPoints_ = numPoints;
        if (node.byteSize_ == 0)
            node.numPoints_ = 0;

        if (node.type_ == NodeType::Proxy) {
            continue;
        }
        for (auto childIdx = 0; childIdx < 8; ++childIdx) {
            auto childExists = ((1 << childIdx) & childMask) != 0;
            if (childExists)
                continue;

            auto childName = node.name_ + std::to_string(childIdx);
            auto childAABB = createChildAABB(node.bb_, childIdx);
            auto child = std::make_shared<Node>(childName, childAABB, node);

            child->spacing_ = node.spacing_ / 2.0;
            child->level_ = node.level_ + 1;
            node.children_[childIdx] = child;

            nodes[nodePos] = child;
            nodePos++;
        }
    }

    return true;
}

BoundingBoxd PointCloudLoader::createChildAABB(const BoundingBoxd &parentBB, int childIdx) {
    auto min = parentBB.min;
    auto max = parentBB.max;
    auto size = parentBB.getSize();

    if ((childIdx & 0b0001) > 0) {
        min.z += size.z / 2;
    } else {
        max.z -= size.z / 2;
    }

    if ((childIdx & 0b0010) > 0) {
        min.y += size.y / 2;
    } else {
        max.y -= size.y / 2;
    }

    if ((childIdx & 0b0100) > 0) {
        min.x += size.x / 2;
    } else {
        max.x -= size.x / 2;
    }

    return BoundingBoxd(min, max);
}

bool PointCloudLoader::loadPoints() {
    std::map<std::string, Attribute> attributeBuffers;
    std::vector<uint8_t> buffer;
    uint64_t numPoints = 0;

    int bytesPerPoint = 0;
    for (const auto &a: attributes_.attr_) {
        bytesPerPoint += a.size_;
    }

    for (const auto &attr: attributes_.attr_) {
        uint32_t attrOffset=attributes_.getOffset(attr.name_);

        if (attr.name_ == "position") {
            auto scale=attr.scale_;
            auto offset=attr.offset_;
            auto min=attr.min_;
            float *positions = reinterpret_cast<float *>(attrBuffer.buffer.data());
            for (int i = 0; i < numPoints; i++) {
                int pointOffset = i * bytesPerPoint;
                int32_t rawX = readLittleEndian<int32_t>(reinterpret_cast<const char *>(buffer.data()),
                                                         pointOffset +attrOffset + 0);
                int32_t rawY = readLittleEndian<int32_t>(reinterpret_cast<const char *>(buffer.data()),
                                                         pointOffset + attrOffset + 4);
                int32_t rawZ = readLittleEndian<int32_t>(reinterpret_cast<const char *>(buffer.data()),
                                                         pointOffset + attrOffset + 8);

                float x = (rawX * scale.x) + offset.x - min.x;
                float y = (rawY * scale.y) + offset.y - min.y;
                float z = (rawZ * scale.z) + offset.z - min.z;

                positions[3 * i + 0] = x;
                positions[3 * i + 1] = y;
                positions[3 * i + 2] = z;
            }
        }
    }
}

void PointCloudLoader::setTarget(Node &node) {
    target_ = &node;
}

void PointCloudLoader::load() {
    if (!target_ || !callback_) return;

    // Step 1: guard — skip if already loaded or in flight
    if (target_->isLoaded() || target_->isLoading()) return;

    // Step 2: mark in-flight
    target_->setLoading(true);

    try {
        // Step 3: proxy nodes carry a hierarchy chunk, not point data.
        // Our design pre-loads the first chunk upfront so proxies in the
        // flat list have no point data — skip them.
        if (target_->type == NodeType::Proxy || target_->byteSize == 0) {
            target_->setLoading(false);
            return;
        }

        // Step 4: read raw bytes from octree.bin
        std::vector<uint8_t> data(target_->byteSize);
        octreeFile_.seekg(static_cast<std::streamoff>(target_->address));
        octreeFile_.read(reinterpret_cast<char *>(data.data()),
                         static_cast<std::streamsize>(target_->byteSize));

        // Step 5: deliver to callback (NodeManager will call setData + notify painters)
        callback_(std::move(data));
    } catch (const std::exception &) {
        target_->setLoading(false);
        return;
    }

    // Step 6: clear loading flag (data is now set, isLoaded() returns true)
    target_->setLoading(false);
}
