#ifndef PCLITE_NODE_MANAGER_H
#define PCLITE_NODE_MANAGER_H

#include <cstdint>
#include <memory>
#include <vector>
#include "mat.h"
#include "vec3.h"

struct Node;
class Camera;
class ManageStrategy;
class Painter;

class NodeManager {
public:
    NodeManager();
    ~NodeManager();

    void addNode(std::shared_ptr<Node> node);
    // Recursively collect every non-proxy node from the tree rooted at root.
    void addTree(std::shared_ptr<Node> root);
    void addStrategy(std::unique_ptr<ManageStrategy> strategy);
    void addPainter(std::unique_ptr<Painter> painter);

    // Runs all strategies to load/cull nodes, then syncs painters.
    void update(const Camera& camera);

    // Calls paint() on every registered painter.
    void render(const Mat4f& viewMatrix, const Mat4f& projMatrix);

    // Calls paintPick() on every registered painter (see Painter::paintPick).
    void renderPick(const Mat4f& viewMatrix, const Mat4f& projMatrix);

    // Asks every painter to resolve a pick id; returns the first non-null
    // match (see Painter::nodeForId).
    Node* nodeForId(uint32_t id) const;

    // Forwards to every painter's setHighlight (see Painter::setHighlight).
    void setHighlight(Node* node, int pointIndex);

    // Forwards to every painter's setPlaneFit (see Painter::setPlaneFit).
    void setPlaneFit(bool active, const vec3f& center, const vec3f& normal, float radius);

    // Currently-loaded (resident, in the active painter set) nodes whose
    // tight bounding box comes within `radius` of `point` -- used to gather
    // cross-node neighbor candidates for pick-assist plane fitting, since a
    // single node's own KD-tree only knows about its own points.
    std::vector<Node*> nodesNear(const vec3d& point, double radius) const;

private:
    class NodeManagerPrivate;
    std::unique_ptr<NodeManagerPrivate> d_;
};

#endif //PCLITE_NODE_MANAGER_H
