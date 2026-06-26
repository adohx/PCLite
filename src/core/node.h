#ifndef PCLITE_NODE_H
#define PCLITE_NODE_H

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "attributes.h"
#include "bounding_box.h"
#include "kdtree.h"
#include "vec3.h"

enum class NodeType : uint8_t {
    Normal = 0,
    Leaf = 1,
    Proxy = 2,
};

struct Node {
    template <typename T>
    static auto readData(const uint8_t* data,int offset)->T {
        auto addr=data+offset;
        return *reinterpret_cast<const T*>(addr);
    };

    explicit Node(const std::string &name, BoundingBoxd bb,
                  std::shared_ptr<Node> parent = nullptr)
        : name_(name), bb_(bb), parent_(std::move(parent)) { children_.resize(8); }

    // ── Tree ──────────────────────────────────────────────────────────────────
    std::string name_;
    std::shared_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node> > children_;

    // ── Descriptor ────────────────────────────────────────────────────────────
    NodeType type_ = NodeType::Normal;
    uint8_t childMask_ = 0;
    uint32_t numPoints_ = 0;
    uint64_t address_ = 0;
    uint64_t byteSize_ = 0;
    BoundingBoxd bb_;
    BoundingBoxd tightBB_{
        {
            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()
        },
        {
            -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity()
        }
    };
    // Proxy lazy-expansion info
    uint64_t proxyChunkAddr_ = 0;
    uint64_t proxyChunkSize_ = 0;

    int level_ = -1;
    double spacing_ = 0.0;

    // ── State ─────────────────────────────────────────────────────────────────
    bool isLoaded() const { return isLoaded_.load(); }
    bool isLoading() const { return isLoading_.load(); }
    void setLoaded(bool v) { isLoaded_.store(v); }
    void setLoading(bool v) { isLoading_.store(v); }

    void clearData() {
        points_.clear();
        colors_.clear();
        kdtreeBytes_.clear();
        kdTreeCache_.reset();
        isLoaded_.store(false);
    }

    bool setData(const std::vector<uint8_t> &data, const Attributes &attributes);

    std::vector<vec3f> getPoints() const;

    std::vector<vec3f> getColors() const;

    // Non-copying access, for hot paths (e.g. pick-assist plane fitting)
    // that just need to index into the points already resident in memory.
    const std::vector<vec3f>& points() const { return points_; }

    // Raw KD-tree bytes read from "kdtree.bin" by the loader (see
    // PointCloudLoader); not parsed yet at this point, just stashed.
    void setKDTreeBytes(std::vector<uint8_t> bytes) {
        kdtreeBytes_ = std::move(bytes);
        kdTreeCache_.reset();
    }

    // Lazily deserializes kdtreeBytes_ on first call and caches the result
    // (cheap: the expensive build() already happened once, offline, in the
    // converter -- this is just a one-time byte-array parse). Empty if no
    // KD-tree bytes were ever set.
    KDTree3 &kdTree() const {
        if (!kdTreeCache_)
            kdTreeCache_ = std::make_unique<KDTree3>(KDTree3::deserialize(kdtreeBytes_));
        return *kdTreeCache_;
    }

private:
    std::vector<vec3f> decodeBlock(const std::vector<uint8_t> &data,
                                   uint64_t N,
                                   const Attribute &attr,
                                   uint64_t blockOff) const;

private:
    std::atomic<bool> isLoaded_{false};
    std::atomic<bool> isLoading_{false};
    Attributes attributes_;
    std::vector<vec3f> points_;
    std::vector<vec3f> colors_;
    std::vector<uint8_t> kdtreeBytes_;
    mutable std::unique_ptr<KDTree3> kdTreeCache_;
};

#endif //PCLITE_NODE_H
