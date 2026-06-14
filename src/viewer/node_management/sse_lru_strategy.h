#ifndef PCLITE_SSE_LRU_STRATEGY_H
#define PCLITE_SSE_LRU_STRATEGY_H

#include "manage_strategy.h"
#include <list>
#include <unordered_map>

// Loading: SSE-based octree traversal.  Starts from each root node and
// recurses into children only while the node's projected inter-point spacing
// exceeds sseThreshold pixels.  Proxy nodes are expanded whenever they fall
// inside the view frustum.
//
// Eviction: an LRU list tracks which loaded nodes were visited this frame.
// When the number of tracked nodes exceeds maxLoadedNodes the least-recently-
// seen nodes are dropped from the returned active set; NodeManager then culls
// their data.
class SSELruStrategy : public ManageStrategy {
public:
    explicit SSELruStrategy(int    screenHeight    = 1080,
                             float  sseThreshold   = 1.0f,
                             size_t maxLoadedNodes  = 1000);

    std::vector<Node*> evaluate(
        const Camera& camera,
        const std::vector<std::shared_ptr<Node>>& nodes) override;

    // Tuning helpers (can be called at any time).
    void setScreenHeight(int h)          { screenHeight_    = h; }
    void setSSEThreshold(float t)        { sseThreshold_    = t; }
    void setMaxLoadedNodes(size_t n)     { maxLoadedNodes_  = n; }

private:
    void  extractFrustumPlanes(const Camera& cam, float planes[6][4]) const;
    bool  isInFrustum(const BoundingBoxd& bb, const float planes[6][4]) const;
    float computeSSE(const Node& node, double distSq, float screenFactor) const;
    void  touchNode(Node* node);
    void  traverse(Node* node, const vec3d& camPos,
                   const float planes[6][4], float screenFactor,
                   std::vector<Node*>& toLoad);

    int    screenHeight_;
    float  sseThreshold_;
    size_t maxLoadedNodes_;

    // LRU: front = most recently used, back = oldest
    std::list<Node*>                                      lruOrder_;
    std::unordered_map<Node*, std::list<Node*>::iterator> lruPos_;
};

#endif // PCLITE_SSE_LRU_STRATEGY_H
