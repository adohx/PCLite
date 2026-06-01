#ifndef PCLITE_ORBIT_CONTROLLER_H
#define PCLITE_ORBIT_CONTROLLER_H

#include "camera_controller.h"
#include "vec3.h"

// Orbit (turntable) camera controller.
//
// Mouse button 1 (left)  + drag  → rotate (azimuth / elevation)
// Mouse button 3 (right) + drag  → pan (translate target in screen plane)
// Scroll wheel                   → zoom (change distance)
class OrbitController : public CameraController {
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
};

#endif // PCLITE_ORBIT_CONTROLLER_H
