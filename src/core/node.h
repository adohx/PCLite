#ifndef PCLITE_NODE_H
#define PCLITE_NODE_H

#include <cstdint>
#include <vector>
#include "vec3.h"

enum class NodeType : uint8_t {
    Normal = 0,
    Leaf   = 1,
    Proxy  = 2,
};

class Node {
public:
    Node(NodeType type, uint8_t childMask, uint32_t pointCount,
         uint64_t address, uint64_t byteSize, vec3d min, vec3d max);
    ~Node();

    NodeType type() const;
    uint8_t childMask() const;
    uint32_t pointCount() const;
    uint64_t address() const;
    uint64_t byteSize() const;
    vec3d min() const;
    vec3d max() const;

    bool isLoaded() const;
    const std::vector<uint8_t>& data() const;
    void setData(std::vector<uint8_t> data);
    void clearData();

private:
    NodeType type_;
    uint8_t childMask_;
    uint32_t pointCount_;
    uint64_t address_;
    uint64_t byteSize_;
    vec3d min_;
    vec3d max_;
    std::vector<uint8_t> data_;
};

#endif //PCLITE_NODE_H
