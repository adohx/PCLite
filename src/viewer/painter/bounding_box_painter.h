#ifndef PCLITE_BOUNDING_BOX_PAINTER_H
#define PCLITE_BOUNDING_BOX_PAINTER_H

#include "painter.h"
#include "vec3.h"
#include <unordered_map>
#include <utility>

// Draws the axis-aligned bounding box that encloses all added nodes.
class BoundingBoxPainter : public Painter {
public:
    void addNode(Node* node)    override;
    void removeNode(Node* node) override;
    void paint(const Mat4f& viewMatrix) override;

private:
    // Per-node bbox stored so removal can correctly shrink the overall box
    std::unordered_map<Node*, std::pair<vec3d, vec3d>> nodeBBoxes_;

    vec3d overallMin_{};
    vec3d overallMax_{};
    bool  dirty_ = true;

    void recompute();
};

#endif //PCLITE_BOUNDING_BOX_PAINTER_H
