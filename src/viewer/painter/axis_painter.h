#ifndef PCLITE_AXIS_PAINTER_H
#define PCLITE_AXIS_PAINTER_H

#include "painter.h"

// Draws X/Y/Z coordinate axes with labels in the bottom-right corner,
// oriented to match the view rotation passed to paint().
class AxisPainter : public Painter {
public:
    void addNode(Node*)    override {}
    void removeNode(Node*) override {}
    void paint(const Mat4f& viewMatrix) override;
};

#endif //PCLITE_AXIS_PAINTER_H
