#ifndef PCLITE_NODE_PAINTER_H
#define PCLITE_NODE_PAINTER_H

#include "painter.h"
#include "attributes.h"
#include "shader.h"
#include <cstdint>
#include <unordered_map>

class NodePainter : public Painter {
public:
    explicit NodePainter(Attributes attributes);
    ~NodePainter() override;

    void addNode(Node* node)    override;
    void removeNode(Node* node) override;
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;
    void paintPick(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

    // Looks up the node a pick pass id refers to (nullptr if it's gone,
    // e.g. culled between the pick render and the readback).
    Node* nodeForId(uint32_t nodeId) const override;

    // Highlights a single point (by its index within node->getPoints()) on
    // subsequent paint() calls. Pass node == nullptr to clear.
    void setHighlight(Node* node, int pointIndex) override;

    // GL point size (in pixels) for non-highlighted points; the highlighted
    // point is always drawn 4x larger (see kPointCloudVertexSrc). Applied on
    // the next paint() call.
    void setPointSize(float size) { pointSize_ = size; }
    float pointSize() const { return pointSize_; }

    // Sum of point counts across every currently resident (added) node --
    // i.e. how many points are actually being drawn this frame, as opposed
    // to the full dataset's point count.
    uint64_t visiblePointCount() const;

private:
    Attributes attributes_;
    Shader shader_;
    Shader pickShader_;

    // GPU-resident geometry for one node's points; uploaded once in addNode,
    // released in removeNode/the destructor.
    struct Batch {
        unsigned int vao = 0, vboPos = 0, vboColor = 0;
        int count = 0;
        uint32_t id = 0;
    };

    std::unordered_map<Node*, Batch> batches_;
    std::unordered_map<uint32_t, Node*> idToNode_;
    uint32_t nextNodeId_ = 1; // 0 is reserved as the "no hit" sentinel

    Node* highlightNode_ = nullptr;
    int highlightIndex_ = -1;
    float pointSize_ = 2.f;

    Batch createBatch(const Node* node) const;
    static void destroyBatch(Batch& batch);
};

#endif //PCLITE_NODE_PAINTER_H
