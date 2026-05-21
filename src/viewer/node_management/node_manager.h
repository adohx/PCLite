#ifndef PCLITE_NODE_MANAGER_H
#define PCLITE_NODE_MANAGER_H

#include <memory>

class Node;
class Camera;
class ManageStrategy;
class Painter;

class NodeManager {
public:
    NodeManager();
    ~NodeManager();

    void addNode(std::unique_ptr<Node> node);
    void addStrategy(std::unique_ptr<ManageStrategy> strategy);
    void addPainter(std::unique_ptr<Painter> painter);

    // Runs all strategies to load/cull nodes, then syncs painters.
    void update(const Camera& camera);

    // Calls paint() on every registered painter.
    void render();

private:
    class NodeManagerPrivate;
    std::unique_ptr<NodeManagerPrivate> d_;
};

#endif //PCLITE_NODE_MANAGER_H
