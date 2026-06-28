#ifndef PCLITE_ARCBALL_CONTROLLER_H
#define PCLITE_ARCBALL_CONTROLLER_H

#include "camera_controller.h"
#include "vec3.h"

// Shoemake Arcball controller.
//
// Each mouse position is projected onto a virtual unit sphere centered on the
// viewport.  The rotation from the previous frame's sphere point to the current
// one is applied directly to the arm vector — no decomposition into separate
// horizontal / vertical steps, so there is no gimbal lock at the poles.
//
// position_/target_/up_ together define exactly what's rendered each frame
// (Camera::viewMatrix() always looks from position_ straight at target_).
// pivot_ is a separate point that onMouseDrag's rotation revolves around;
// it defaults to target_ (so by default this behaves exactly like a
// classic "orbit around what you're looking at" arcball). recenterTo()
// only ever touches pivot_ -- never position_/target_/up_ -- so retargeting
// the pivot (e.g. to a just-double-clicked point) can never move the
// camera or change what's on screen; only how the *next* drag revolves it.
//
// Left drag  : rotate position_ and target_ together, rigidly, around pivot_
// Right drag : pan (move target + camera together)
// Scroll     : zoom (move camera along the position→target line)
class ArcballController : public CameraController {
public:
    void onMouseButtonDown(int button, float x, float y) override;
    void onMouseDrag(int button, float dx, float dy) override;
    void recenterTo(vec3d point) override;
    void clearRecenter() override;
    void onScroll(float delta) override;
    void onResize(int w, int h) override;
    void syncFromCamera(const Camera& cam) override;
    void applyToCamera(Camera& cam) const override;

private:
    vec3d position_ = {0.0, 0.0, 50.0};
    vec3d target_   = {0.0, 0.0,  0.0};
    vec3d pivot_    = {0.0, 0.0,  0.0}; // orbit center for onMouseDrag; == target_ unless recenterTo() was called
    vec3d up_       = {0.0, 1.0,  0.0};
    float fov_    = 60.f;
    int   width_  = 800;
    int   height_ = 600;

    // Drag tracking for Shoemake sphere projection
    float prevDragX_ = 0.f;
    float prevDragY_ = 0.f;

    // Project screen position onto the virtual unit sphere (Shoemake mapping)
    vec3d spherePoint(float x, float y) const;

    void applyPan(float dx, float dy);
};

#endif // PCLITE_ARCBALL_CONTROLLER_H
