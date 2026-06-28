#include "sse_lru_strategy.h"
#include "../../core/node.h"
#include "camera/camera.h"
#include "mat.h"
#include <cmath>
#include <limits>

SSELruStrategy::SSELruStrategy(int screenHeight, float sseThreshold, size_t maxLoadedNodes)
    : screenHeight_(screenHeight)
    , sseThreshold_(sseThreshold)
    , maxLoadedNodes_(maxLoadedNodes)
{}

// ── Frustum ───────────────────────────────────────────────────────────────────

void SSELruStrategy::extractFrustumPlanes(const Camera& cam, float planes[6][4]) const {
    // VP = P * V  (row-major storage, column-vector convention)
    Mat4f VP = cam.projectionMatrix() * cam.viewMatrix();

    // Gribb-Hartmann method: plane i is "inside" when dot(plane_i, p) >= 0
    //   Left:   row3 + row0     Right:  row3 - row0
    //   Bottom: row3 + row1     Top:    row3 - row1
    //   Near:   row3 + row2     Far:    row3 - row2
    for (int c = 0; c < 4; ++c) planes[0][c] = VP(3,c) + VP(0,c);
    for (int c = 0; c < 4; ++c) planes[1][c] = VP(3,c) - VP(0,c);
    for (int c = 0; c < 4; ++c) planes[2][c] = VP(3,c) + VP(1,c);
    for (int c = 0; c < 4; ++c) planes[3][c] = VP(3,c) - VP(1,c);
    for (int c = 0; c < 4; ++c) planes[4][c] = VP(3,c) + VP(2,c);
    for (int c = 0; c < 4; ++c) planes[5][c] = VP(3,c) - VP(2,c);
}

bool SSELruStrategy::isInFrustum(const BoundingBoxd& bb, const float planes[6][4]) const {
    auto mn = bb.min();
    auto mx = bb.max();
    for (int i = 0; i < 6; ++i) {
        const float* p = planes[i];
        // Positive vertex: corner furthest in the plane-normal direction
        float px = p[0] >= 0.f ? (float)mx.x : (float)mn.x;
        float py = p[1] >= 0.f ? (float)mx.y : (float)mn.y;
        float pz = p[2] >= 0.f ? (float)mx.z : (float)mn.z;
        if (p[0]*px + p[1]*py + p[2]*pz + p[3] < 0.f)
            return false;
    }
    return true;
}

// ── SSE ───────────────────────────────────────────────────────────────────────

float SSELruStrategy::computeSSE(const Node& node, double distSq, float screenFactor) const {
    double size = node.spacing_;
    if (size <= 0.0) {
        // Fallback: half the bounding-box diagonal
        auto s = node.bb_.getSize();
        size = std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z) * 0.5;
    }
    if (distSq < 1e-10)
        return std::numeric_limits<float>::max();
    return (float)(size * screenFactor / std::sqrt(distSq));
}

// ── LRU ───────────────────────────────────────────────────────────────────────

void SSELruStrategy::touchNode(Node* node) {
    touchedThisFrame_.insert(node);

    auto it = lruPos_.find(node);
    if (it != lruPos_.end()) {
        lruOrder_.erase(it->second);
        lruPos_.erase(it);
    }
    lruOrder_.push_front(node);
    lruPos_[node] = lruOrder_.begin();
}

// ── Octree traversal ──────────────────────────────────────────────────────────

void SSELruStrategy::traverse(Node* node, const vec3d& camPos,
                               const float planes[6][4], float screenFactor,
                               std::vector<Node*>& toLoad) {
    if (!node) return;
    if (!isInFrustum(node->bb_, planes)) return;

    // Distance from camera to node centre
    auto mn = node->bb_.min();
    auto mx = node->bb_.max();
    double cx = (mn.x + mx.x) * 0.5;
    double cy = (mn.y + mx.y) * 0.5;
    double cz = (mn.z + mx.z) * 0.5;
    double dx = camPos.x - cx, dy = camPos.y - cy, dz = camPos.z - cz;
    double distSq = dx*dx + dy*dy + dz*dz;

    if (node->type_ == NodeType::Proxy) {
        // Expand hierarchy chunk before we can recurse further
        if (!node->isLoaded() && !node->isLoading())
            toLoad.push_back(node);
        return;
    }

    // Track or queue this node
    if (node->isLoaded())
        touchNode(node);
    else if (!node->isLoading())
        toLoad.push_back(node);

    // Recurse into children only when finer detail is warranted
    if (computeSSE(*node, distSq, screenFactor) > sseThreshold_) {
        for (auto& child : node->children_)
            if (child) traverse(child.get(), camPos, planes, screenFactor, toLoad);
    }
}

// ── ManageStrategy interface ──────────────────────────────────────────────────

std::vector<Node*> SSELruStrategy::evaluate(
    const Camera& camera,
    const std::vector<std::shared_ptr<Node>>& nodes)
{
    float planes[6][4];
    extractFrustumPlanes(camera, planes);

    float halfFovRad  = camera.fov() * (float)M_PI / 360.0f;
    float screenFactor = (float)screenHeight_ / (2.0f * std::tan(halfFovRad));
    vec3d camPos = camera.position();

    // Collect nodes to load while touching already-loaded ones in LRU
    std::vector<Node*> toLoad;
    touchedThisFrame_.clear();
    for (auto& n : nodes)
        if (n->parent_ == nullptr)
            traverse(n.get(), camPos, planes, screenFactor, toLoad);

    // Evict LRU tail if over budget (they will be culled by NodeManager) --
    // but only nodes NOT touched this frame. Once the current view needs
    // more distinct nodes than maxLoadedNodes_, evicting one of them just
    // because it was touched earlier in this same frame's traversal order
    // would make it reload from disk next frame only to be evicted again:
    // permanent thrash (visible as the displayed point count oscillating
    // and FPS collapsing under constant disk I/O). Better to temporarily
    // exceed the budget; it self-corrects once the view needs fewer nodes.
    size_t overBudget = lruPos_.size() > maxLoadedNodes_ ? lruPos_.size() - maxLoadedNodes_ : 0;
    if (overBudget > 0) {
        std::vector<Node*> victims;
        victims.reserve(overBudget);
        for (auto rit = lruOrder_.rbegin(); rit != lruOrder_.rend() && victims.size() < overBudget; ++rit)
            if (!touchedThisFrame_.count(*rit)) victims.push_back(*rit);

        for (Node* victim : victims) {
            lruOrder_.erase(lruPos_[victim]);
            lruPos_.erase(victim);
        }
    }

    // Active set = LRU members (recently-seen loaded nodes) + nodes queued for load
    std::vector<Node*> active(lruOrder_.begin(), lruOrder_.end());
    for (Node* n : toLoad)
        active.push_back(n);

    return active;
}
