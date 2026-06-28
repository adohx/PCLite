#ifndef PCLITE_MEASUREMENT_PAINTER_H
#define PCLITE_MEASUREMENT_PAINTER_H

#include "painter.h"
#include "shader.h"

class MeasurementManager;

// Draws the current measurement's line segments (see
// MeasurementManager::currentLines()), one small GL_LINES draw call per
// segment since there are only ever a handful (<= 3 for distance, a couple
// rays + a short arc for angle) -- not worth batching by color. Numeric
// labels are NOT drawn here: they're plain screen-space text (see
// Viewport::measurementScreenLabels()/MainWindow), since that's the
// simplest way to guarantee they always face the screen, and ImGui's text
// rendering can't run inside this GL pass anyway.
class MeasurementPainter : public Painter {
public:
    // manager is non-owning; must outlive this painter (Viewport owns it
    // for the lifetime of the project, same as the painter's host layer).
    explicit MeasurementPainter(const MeasurementManager* manager);
    ~MeasurementPainter() override;

    void addNode(Node*) override {}
    void removeNode(Node*) override {}
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    const MeasurementManager* manager_;
    Shader shader_;
    unsigned int vao_ = 0, vbo_ = 0;
};

#endif //PCLITE_MEASUREMENT_PAINTER_H
