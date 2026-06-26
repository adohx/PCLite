#ifndef PCLITE_PAINTER_H
#define PCLITE_PAINTER_H

#include "../../core/node.h"
#include "mat.h"

class Camera;

class Painter {
public:
    virtual ~Painter() = default;

    virtual void addNode(Node* node) = 0;
    virtual void removeNode(Node* node) = 0;
    virtual void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) = 0;

    // Called once per frame with the up-to-date camera, before paint().
    // Most painters don't need camera state directly (it's baked into the
    // view/proj matrices); this is only for painters that need camera-only
    // data the matrices don't carry, e.g. the orbit target point.
    virtual void syncCamera(const Camera& /*camera*/) {}
};

#endif //PCLITE_PAINTER_H
