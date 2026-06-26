#ifndef PCLITE_AXIS_PAINTER_H
#define PCLITE_AXIS_PAINTER_H

#include "painter.h"
#include "shader.h"

// Draws X/Y/Z coordinate axes with labels in the bottom-right corner,
// oriented to match the view rotation passed to paint().
class AxisPainter : public Painter {
public:
    AxisPainter();
    ~AxisPainter() override;

    void addNode(Node*)    override {}
    void removeNode(Node*) override {}
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    Shader shader_;

    // Static geometry (origin/tip per axis, and the 3 tip points) never
    // changes, so it's uploaded once instead of re-emitted every frame.
    unsigned int vaoAxisLines_ = 0, vboAxisLines_ = 0;
    unsigned int vaoAxisPoints_ = 0, vboAxisPoints_ = 0;

    // The projected letter-glyph geometry depends on the camera rotation, so
    // it's rebuilt and re-uploaded every paint() call.
    unsigned int vaoLabels_ = 0, vboLabels_ = 0;
};

#endif //PCLITE_AXIS_PAINTER_H
