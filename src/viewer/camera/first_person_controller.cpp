#include "first_person_controller.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

// yaw=0 → looking toward -Z; yaw increases clockwise from above.
vec3d FirstPersonController::forwardVec() const {
    float y = yaw_   * (float)M_PI / 180.f;
    float p = pitch_ * (float)M_PI / 180.f;
    return {
         std::sin(y) * std::cos(p),
         std::sin(p),
        -std::cos(y) * std::cos(p)
    };
}

// Right vector: perpendicular to forward in the horizontal plane.
vec3d FirstPersonController::rightVec() const {
    float y = yaw_ * (float)M_PI / 180.f;
    return {std::cos(y), 0.0, std::sin(y)};
}

void FirstPersonController::onMouseDrag(int button, float dx, float dy) {
    if (button == kLeftButton) {
        static constexpr float kSens = 0.2f;  // degrees per pixel
        yaw_   += dx * kSens;
        pitch_  = std::clamp(pitch_ - dy * kSens, -89.f, 89.f);
        return;
    }

    if (button == kRightButton) {
        // Pan: move in screen right / up direction
        vec3d fwd = forwardVec();
        vec3d rgt = rightVec();
        // Screen up = cross(right, forward), approximated as world Y tilted by pitch
        vec3d up = {
            -std::sin(pitch_ * (float)M_PI / 180.f) * std::sin(yaw_ * (float)M_PI / 180.f),
             std::cos(pitch_ * (float)M_PI / 180.f),
             std::sin(pitch_ * (float)M_PI / 180.f) * std::cos(yaw_ * (float)M_PI / 180.f)
        };

        float fovRad = fov_ * (float)M_PI / 180.f;
        // Use a fixed reference distance for pan scale (10 units feels natural)
        float scale  = 2.f * 10.f * std::tan(fovRad * 0.5f) / (float)height_;

        position_.x += (double)(-dx * scale * rgt.x + dy * scale * up.x);
        position_.y += (double)(-dx * scale * rgt.y + dy * scale * up.y);
        position_.z += (double)(-dx * scale * rgt.z + dy * scale * up.z);
    }
}

void FirstPersonController::onScroll(float delta) {
    // Move forward/backward along view direction
    vec3d fwd = forwardVec();
    float step = speed_ * 0.5f * (delta > 0.f ? 1.f : -1.f);
    position_.x += fwd.x * step;
    position_.y += fwd.y * step;
    position_.z += fwd.z * step;
}

void FirstPersonController::onResize(int /*w*/, int h) {
    height_ = h;
}

void FirstPersonController::onKey(int key, bool pressed) {
    if (pressed) heldKeys_.insert(key);
    else         heldKeys_.erase(key);
}

void FirstPersonController::update(float dt) {
    if (heldKeys_.empty()) return;

    vec3d fwd = forwardVec();
    vec3d rgt = rightVec();

    float d = speed_ * dt;

    auto move = [&](int key, double ax, double ay, double az) {
        if (!heldKeys_.count(key)) return;
        position_.x += ax * d;
        position_.y += ay * d;
        position_.z += az * d;
    };

    move(SDLK_w,  fwd.x, fwd.y, fwd.z);
    move(SDLK_s, -fwd.x,-fwd.y,-fwd.z);
    move(SDLK_d,  rgt.x, rgt.y, rgt.z);
    move(SDLK_a, -rgt.x,-rgt.y,-rgt.z);
    move(SDLK_e,  0, 1, 0);
    move(SDLK_q,  0,-1, 0);
}

void FirstPersonController::syncFromCamera(const Camera& cam) {
    fov_      = cam.fov();
    position_ = cam.position();

    auto tgt = cam.target();
    float dx = (float)(tgt.x - position_.x);
    float dy = (float)(tgt.y - position_.y);
    float dz = (float)(tgt.z - position_.z);
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) return;

    pitch_ = std::asin(std::clamp(dy / len, -1.f, 1.f)) * 180.f / (float)M_PI;
    yaw_   = std::atan2(dx, -dz) * 180.f / (float)M_PI;
}

void FirstPersonController::applyToCamera(Camera& cam) const {
    vec3d fwd = forwardVec();
    vec3d target = {
        position_.x + fwd.x,
        position_.y + fwd.y,
        position_.z + fwd.z
    };
    cam.setPosition(position_);
    cam.setTarget(target);
    cam.setPivot(target);
}
