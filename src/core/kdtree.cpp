#include "kdtree.h"

#include <algorithm>
#include <cstring>
#include <numeric>

namespace {

float axisOf(const vec3f& v, int axis) {
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

template <class T>
void pushBytes(std::vector<uint8_t>& out, const T& v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

template <class T>
T readBytes(const uint8_t* data, size_t& offset) {
    T v{};
    std::memcpy(&v, data + offset, sizeof(T));
    offset += sizeof(T);
    return v;
}

} // namespace

int KDTree3::buildRecursive(std::vector<uint32_t>& indices, size_t begin, size_t end,
                             const std::vector<vec3f>& points, int depth) {
    if (begin >= end) return -1;

    size_t mid = begin + (end - begin) / 2;
    int axis = depth % 3;

    std::nth_element(indices.begin() + begin, indices.begin() + mid, indices.begin() + end,
                      [&](uint32_t a, uint32_t b) {
                          return axisOf(points[a], axis) < axisOf(points[b], axis);
                      });

    uint32_t pointIdx = indices[mid];
    const vec3f& p = points[pointIdx];

    int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(Node{p.x, p.y, p.z, pointIdx, static_cast<uint8_t>(axis), -1, -1});

    int left  = buildRecursive(indices, begin, mid, points, depth + 1);
    int right = buildRecursive(indices, mid + 1, end, points, depth + 1);
    nodes_[nodeIdx].left  = left;
    nodes_[nodeIdx].right = right;
    return nodeIdx;
}

void KDTree3::build(const std::vector<vec3f>& points) {
    nodes_.clear();
    root_ = -1;
    if (points.empty()) return;

    nodes_.reserve(points.size());
    std::vector<uint32_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0u);
    root_ = buildRecursive(indices, 0, indices.size(), points, 0);
}

void KDTree3::searchKNN(int nodeIdx, const vec3f& query, int k, std::vector<Candidate>& best) const {
    if (nodeIdx < 0) return;
    const Node& n = nodes_[nodeIdx];

    auto cmp = [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; };

    float dx = n.x - query.x, dy = n.y - query.y, dz = n.z - query.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    if (static_cast<int>(best.size()) < k) {
        best.push_back({distSq, n.pointIndex});
        std::push_heap(best.begin(), best.end(), cmp);
    } else if (distSq < best.front().distSq) {
        std::pop_heap(best.begin(), best.end(), cmp);
        best.back() = {distSq, n.pointIndex};
        std::push_heap(best.begin(), best.end(), cmp);
    }

    float diff = axisOf(query, n.axis) - axisOf({n.x, n.y, n.z}, n.axis);
    int nearChild = diff <= 0 ? n.left : n.right;
    int farChild  = diff <= 0 ? n.right : n.left;

    searchKNN(nearChild, query, k, best);
    if (static_cast<int>(best.size()) < k || diff * diff < best.front().distSq)
        searchKNN(farChild, query, k, best);
}

std::vector<uint32_t> KDTree3::kNearest(const vec3f& query, int k) const {
    std::vector<Candidate> best;
    if (k <= 0 || root_ < 0) return {};
    best.reserve(k);
    searchKNN(root_, query, k, best);

    std::sort(best.begin(), best.end(),
              [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; });

    std::vector<uint32_t> out;
    out.reserve(best.size());
    for (const Candidate& c : best) out.push_back(c.pointIndex);
    return out;
}

void KDTree3::searchRadius(int nodeIdx, const vec3f& query, float radiusSq,
                            std::vector<uint32_t>& out) const {
    if (nodeIdx < 0) return;
    const Node& n = nodes_[nodeIdx];

    float dx = n.x - query.x, dy = n.y - query.y, dz = n.z - query.z;
    if (dx * dx + dy * dy + dz * dz <= radiusSq) out.push_back(n.pointIndex);

    float diff = axisOf(query, n.axis) - axisOf({n.x, n.y, n.z}, n.axis);
    int nearChild = diff <= 0 ? n.left : n.right;
    int farChild  = diff <= 0 ? n.right : n.left;

    searchRadius(nearChild, query, radiusSq, out);
    if (diff * diff <= radiusSq) searchRadius(farChild, query, radiusSq, out);
}

std::vector<uint32_t> KDTree3::radiusSearch(const vec3f& query, float radius) const {
    std::vector<uint32_t> out;
    if (root_ < 0 || radius <= 0.f) return out;
    searchRadius(root_, query, radius * radius, out);
    return out;
}

std::vector<uint8_t> KDTree3::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(8 + nodes_.size() * 25);

    pushBytes(out, static_cast<uint32_t>(nodes_.size()));
    pushBytes(out, static_cast<int32_t>(root_));

    for (const Node& n : nodes_) {
        pushBytes(out, n.x);
        pushBytes(out, n.y);
        pushBytes(out, n.z);
        pushBytes(out, n.pointIndex);
        pushBytes(out, n.axis);
        pushBytes(out, n.left);
        pushBytes(out, n.right);
    }
    return out;
}

KDTree3 KDTree3::deserialize(const std::vector<uint8_t>& bytes) {
    KDTree3 tree;
    size_t offset = 0;
    if (bytes.size() < 8) return tree;

    uint32_t count = readBytes<uint32_t>(bytes.data(), offset);
    tree.root_ = readBytes<int32_t>(bytes.data(), offset);

    tree.nodes_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        Node n;
        n.x = readBytes<float>(bytes.data(), offset);
        n.y = readBytes<float>(bytes.data(), offset);
        n.z = readBytes<float>(bytes.data(), offset);
        n.pointIndex = readBytes<uint32_t>(bytes.data(), offset);
        n.axis = readBytes<uint8_t>(bytes.data(), offset);
        n.left = readBytes<int32_t>(bytes.data(), offset);
        n.right = readBytes<int32_t>(bytes.data(), offset);
        tree.nodes_[i] = n;
    }
    return tree;
}
