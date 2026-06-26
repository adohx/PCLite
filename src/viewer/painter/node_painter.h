#ifndef PCLITE_NODE_PAINTER_H
#define PCLITE_NODE_PAINTER_H

#include "painter.h"
#include "attributes.h"
#include "shader.h"
#include <unordered_map>

class NodePainter : public Painter {
public:
    explicit NodePainter(Attributes attributes);
    ~NodePainter() override;

    void addNode(Node* node)    override;
    void removeNode(Node* node) override;
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    Attributes attributes_;
    Shader shader_;

    // GPU-resident geometry for one node's points; uploaded once in addNode,
    // released in removeNode/the destructor.
    struct Batch {
        unsigned int vao = 0, vboPos = 0, vboColor = 0;
        int count = 0;
    };

    std::unordered_map<Node*, Batch> batches_;

    Batch createBatch(const Node* node) const;
    static void destroyBatch(Batch& batch);
};

#endif //PCLITE_NODE_PAINTER_H
