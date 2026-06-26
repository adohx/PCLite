#ifndef PCLITE_KDTREE_H
#define PCLITE_KDTREE_H

#include "vec3.h"
#include <cstdint>
#include <vector>

// Self-contained balanced KD-tree over 3D points: each tree node carries its
// own copy of the point's position and its original index, so a
// deserialized tree needs no external point array to answer queries (the
// converter builds it once from decoded positions; the viewer just
// deserializes the bytes and queries it).
class KDTree3 {
public:
    void build(const std::vector<vec3f>& points);

    bool empty() const { return nodes_.empty(); }

    // Indices (into the points array passed to build()) of the k nearest
    // neighbors of query, nearest first.
    std::vector<uint32_t> kNearest(const vec3f& query, int k) const;

    // Indices of all points within `radius` of query (unsorted).
    std::vector<uint32_t> radiusSearch(const vec3f& query, float radius) const;

    std::vector<uint8_t> serialize() const;
    static KDTree3 deserialize(const std::vector<uint8_t>& bytes);

private:
    struct Node {
        float x = 0, y = 0, z = 0;
        uint32_t pointIndex = 0;
        uint8_t axis = 0;
        int32_t left = -1, right = -1;
    };

    int buildRecursive(std::vector<uint32_t>& indices, size_t begin, size_t end,
                        const std::vector<vec3f>& points, int depth);

    struct Candidate { float distSq; uint32_t pointIndex; };
    void searchKNN(int nodeIdx, const vec3f& query, int k, std::vector<Candidate>& best) const;
    void searchRadius(int nodeIdx, const vec3f& query, float radiusSq, std::vector<uint32_t>& out) const;

    std::vector<Node> nodes_;
    int root_ = -1;
};

#endif //PCLITE_KDTREE_H
