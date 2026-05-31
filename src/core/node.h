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
#include "vec3.h"

enum class NodeType : uint8_t {
    Normal = 0,
    Leaf = 1,
    Proxy = 2,
};

struct Node {
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
        isLoaded_.store(false);
    }

    bool setData(const std::vector<uint8_t> &data, const Attributes &attributes);

    std::vector<vec3f> getPoints() const;

    std::vector<vec3f> getColors() const;

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
};

#endif //PCLITE_NODE_H
