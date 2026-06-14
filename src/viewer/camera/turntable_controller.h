#ifndef PCLITE_TURNTABLE_CONTROLLER_H
#define PCLITE_TURNTABLE_CONTROLLER_H

#include "camera_controller.h"
#include "vec3.h"

// Turntable orbit controller.
// State: azimuth/elevation/distance (spherical coordinates).
//
// Left drag  : azimuth (world Y) + elevation (clamped to ±89°)
// Right drag : pan
// Scroll     : zoom
class TurntableController : public CameraController {
public:
    void onMouseDrag(int button, float dx, float dy) override;
    void onScroll(float delta) override;
    void onResize(int w, int h) override;
    void syncFromCamera(const Camera& cam) override;
    void applyToCamera(Camera& cam) const override;

private:
    float azimuth_   = 45.f;
    float elevation_ = 30.f;
    float distance_  = 50.f;
    vec3d target_    = {0.0, 0.0, 0.0};
    float fov_       = 60.f;
    int   height_    = 600;

    void applyPan(float dx, float dy);
};

#endif // PCLITE_TURNTABLE_CONTROLLER_H
