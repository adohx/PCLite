#ifndef PCLITE_FIRST_PERSON_CONTROLLER_H
#define PCLITE_FIRST_PERSON_CONTROLLER_H

#include "camera_controller.h"
#include "vec3.h"
#include <unordered_set>

// First-person (fly) camera controller.
//
// Left drag          → look around (yaw / pitch)
// Right drag         → pan in screen plane
// WASD               → move forward / back / left / right
// Q / E              → move down / up
// Scroll             → move forward / backward quickly
class FirstPersonController : public CameraController {
public:
    void onMouseDrag(int button, float dx, float dy) override;
    void onScroll(float delta) override;
    void onResize(int w, int h) override;
    void onKey(int key, bool pressed) override;
    void update(float dt) override;
    void syncFromCamera(const Camera& cam) override;
    void applyToCamera(Camera& cam) const override;

    void setMoveSpeed(float s) { speed_ = s; }

private:
    vec3d position_ = {0.0, 0.0, 10.0};
    float yaw_      = 0.f;   // degrees; 0 = looking toward -Z
    float pitch_    = 0.f;   // degrees; positive = looking up

    float speed_    = 10.f;
    float fov_      = 60.f;
    int   height_   = 600;

    std::unordered_set<int> heldKeys_;

    vec3d forwardVec() const;
    vec3d rightVec()   const;
};

#endif // PCLITE_FIRST_PERSON_CONTROLLER_H
