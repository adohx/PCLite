#ifndef PCLITE_NODE_PAINTER_H
#define PCLITE_NODE_PAINTER_H

#include "painter.h"
#include "attributes.h"
#include <unordered_map>
#include <vector>

class NodePainter : public Painter {
public:
    explicit NodePainter(Attributes attributes);

    void addNode(Node* node)    override;
    void removeNode(Node* node) override;
    void paint(const Mat4f& viewMatrix) override;

private:
    Attributes attributes_;

    struct Batch {
        std::vector<float> positions;
        std::vector<float> colors;
    };

    std::unordered_map<Node*, Batch> batches_;

    Batch decode(const Node* node) const;
};

#endif //PCLITE_NODE_PAINTER_H
