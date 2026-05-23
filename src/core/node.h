#ifndef PCLITE_NODE_H
#define PCLITE_NODE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "bounding_box.h"
#include "vec3.h"

enum class NodeType : uint8_t {
    Normal = 0,
    Leaf = 1,
    Proxy = 2,
};

struct Node {
    explicit Node(const std::string &name, BoundingBoxd bb, std::shared_ptr<Node> parent = std::shared_ptr<Node>())
        : name_(name), bb_(bb), parent_(parent) {
        children_.resize(8);
    }

    std::shared_ptr<Node> parent_;
    std::string name_{};
    NodeType type_;
    uint8_t childMask_;
    uint32_t numPoints_;

    BoundingBoxd bb_;
    std::vector<uint8_t> data;

    int level_ = -1;
    uint64_t proxyChunkAddr_ = 0;
    uint64_t proxyChunkSize_ = 0;
    double spacing_ = 0.0;
    uint64_t address_;
    uint64_t byteSize_;


    bool isLoaded() const { return isLoaded_.load(); }
    bool isLoading() const { return isLoading_.load(); }
    void setLoading(bool v) { isLoading_.store(v); }
    void setLoaded(bool v) { isLoaded_.store(v); }

    std::vector<std::shared_ptr<Node> > children_;

private:
    std::atomic<bool> isLoading_ = false;
    std::atomic<bool> isLoaded_ = false;
};

#endif //PCLITE_NODE_H
