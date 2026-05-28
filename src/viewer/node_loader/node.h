#ifndef PCLITE_NODE_H
#define PCLITE_NODE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../core/attributes.h"
#include "../../core/bounding_box.h"
#include "../../core/vec3.h"

enum class NodeType : uint8_t {
    Normal = 0,
    Leaf   = 1,
    Proxy  = 2,
};

struct Node {
    explicit Node(const std::string& name, BoundingBoxd bb,
                  std::shared_ptr<Node> parent = nullptr)
        : name_(name), bb_(bb), parent_(std::move(parent))
    { children_.resize(8); }

    // ── Tree ──────────────────────────────────────────────────────────────────
    std::string name_;
    std::shared_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node>> children_;

    // ── Descriptor ────────────────────────────────────────────────────────────
    NodeType type_     = NodeType::Normal;
    uint8_t  childMask_ = 0;
    uint32_t numPoints_ = 0;
    uint64_t address_   = 0;
    uint64_t byteSize_  = 0;
    BoundingBoxd bb_;

    // Proxy lazy-expansion info
    uint64_t proxyChunkAddr_ = 0;
    uint64_t proxyChunkSize_ = 0;

    int    level_   = -1;
    double spacing_ = 0.0;

    // ── Point data (raw bytes from octree.bin) ────────────────────────────────
    std::vector<uint8_t> data_;

    // ── State ─────────────────────────────────────────────────────────────────
    bool isLoaded()  const { return isLoaded_.load(); }
    bool isLoading() const { return isLoading_.load(); }
    void setLoaded(bool v)  { isLoaded_.store(v); }
    void setLoading(bool v) { isLoading_.store(v); }

private:
    std::atomic<bool> isLoaded_{false};
    std::atomic<bool> isLoading_{false};
};

#endif //PCLITE_NODE_H
