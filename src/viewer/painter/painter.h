#ifndef PCLITE_PAINTER_H
#define PCLITE_PAINTER_H

#include "../../core/node.h"
#include "mat.h"

class Painter {
public:
    virtual ~Painter() = default;

    virtual void addNode(Node* node) = 0;
    virtual void removeNode(Node* node) = 0;
    virtual void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) = 0;
};

#endif //PCLITE_PAINTER_H
