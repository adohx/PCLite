 #ifndef PCLITE_ROTATION_CENTER_PAINTER_H
#define PCLITE_ROTATION_CENTER_PAINTER_H

#include "painter.h"
#include "shader.h"
#include "vec3.h"

// Draws a small 3-axis crosshair at the controller's orbit pivot (Camera::
// pivot(), set by ArcballController et al. -- see CameraController::
// recenterTo()), so the user can see where that point is in the scene, and
// see it move immediately when they double-click to re-pivot.
class RotationCenterPainter : public Painter {
public:
    RotationCenterPainter();
    ~RotationCenterPainter() override;

    void addNode(Node*)    override {}
    void removeNode(Node*) override {}
    void syncCamera(const Camera& camera) override;
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    vec3d pivot_          = {0.0, 0.0, 0.0};
    float cameraDistance_ = 50.f; // distance from camera to pivot; sizes the crosshair

    Shader shader_;
    unsigned int vao_ = 0, vbo_ = 0;

    void rebuildGeometry();
};

#endif // PCLITE_ROTATION_CENTER_PAINTER_H
