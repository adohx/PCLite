 #ifndef PCLITE_ROTATION_CENTER_PAINTER_H
#define PCLITE_ROTATION_CENTER_PAINTER_H

#include "painter.h"
#include "shader.h"
#include "vec3.h"

// Draws a small 3-axis crosshair at the camera's orbit target (the point the
// turntable/arcball controllers rotate and zoom around), so the user can see
// where that point is in the scene.
class RotationCenterPainter : public Painter {
public:
    RotationCenterPainter();
    ~RotationCenterPainter() override;

    void addNode(Node*)    override {}
    void removeNode(Node*) override {}
    void syncCamera(const Camera& camera) override;
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    vec3d target_         = {0.0, 0.0, 0.0};
    float cameraDistance_ = 50.f; // distance from camera to target; sizes the crosshair

    Shader shader_;
    unsigned int vao_ = 0, vbo_ = 0;

    void rebuildGeometry();
};

#endif // PCLITE_ROTATION_CENTER_PAINTER_H
