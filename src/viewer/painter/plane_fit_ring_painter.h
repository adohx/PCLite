#ifndef PCLITE_PLANE_FIT_RING_PAINTER_H
#define PCLITE_PLANE_FIT_RING_PAINTER_H

#include "painter.h"
#include "shader.h"
#include "vec3.h"

// Pick-assist indicator: draws a flat annulus (two concentric circles --
// transparent inside the inner one, translucent color in the band between
// inner and outer) centered at the locally-fitted plane through the last
// picked point's neighborhood, oriented to match the plane's normal so it
// visually "lies flat" on the surface under the cursor. Sized to a constant
// screen-space footprint (like RotationCenterPainter's crosshair) rather
// than the kNN search radius, so it reads as a fixed cursor decal regardless
// of zoom level.
class PlaneFitRingPainter : public Painter {
public:
    PlaneFitRingPainter();
    ~PlaneFitRingPainter() override;

    void addNode(Node*)    override {}
    void removeNode(Node*) override {}
    void setPlaneFit(bool active, const vec3f& center, const vec3f& normal, float radius) override;
    void syncCamera(const Camera& camera) override;
    void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) override;

private:
    static constexpr int kSegments = 48;
    // Outer/inner radius as a fraction of camera distance, for constant
    // apparent screen size (same trick as RotationCenterPainter's crosshair).
    static constexpr float kOuterFraction = 0.05f;
    static constexpr float kInnerFraction = 0.7f; // of the outer radius

    bool active_ = false;
    vec3f center_{0, 0, 0};
    vec3f normal_{0, 0, 1};
    float cameraDistance_ = 50.f;

    Shader shader_;
    unsigned int vao_ = 0, vbo_ = 0;

    void rebuildGeometry();
};

#endif //PCLITE_PLANE_FIT_RING_PAINTER_H
