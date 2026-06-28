#include "turntable_controller.h"
#include <algorithm>
#include <cmath>

static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

void TurntableController::onMouseDrag(int button, float dx, float dy) {
    if (button == kLeftButton) {
        azimuth_   -= dx * 0.3f;
        elevation_  = std::clamp(elevation_ - dy * 0.3f, -89.f, 89.f);
        return;
    }
    if (button == kRightButton) applyPan(dx, dy);
}

void TurntableController::onScroll(float delta) {
    distance_ *= (delta > 0.f) ? 0.85f : 1.18f;
    distance_  = std::clamp(distance_, 0.5f, 5000.f);
}

void TurntableController::onResize(int /*w*/, int h) {
    height_ = h;
}

void TurntableController::syncFromCamera(const Camera& cam) {
    target_ = cam.target();
    fov_    = cam.fov();

    auto pos = cam.position();
    float dx = (float)(pos.x - target_.x);
    float dy = (float)(pos.y - target_.y);
    float dz = (float)(pos.z - target_.z);

    distance_  = std::sqrt(dx*dx + dy*dy + dz*dz);
    elevation_ = std::asin(std::clamp(dy / distance_, -1.f, 1.f)) * 180.f / (float)M_PI;
    azimuth_   = std::atan2(dx, dz) * 180.f / (float)M_PI;
}

void TurntableController::applyToCamera(Camera& cam) const {
    float az = azimuth_   * (float)M_PI / 180.f;
    float el = elevation_ * (float)M_PI / 180.f;

    cam.setTarget(target_);
    cam.setPosition({
        target_.x + distance_ * std::cos(el) * std::sin(az),
        target_.y + distance_ * std::sin(el),
        target_.z + distance_ * std::cos(el) * std::cos(az)
    });
    cam.setPivot(target_);
}

void TurntableController::applyPan(float dx, float dy) {
    float az = azimuth_   * (float)M_PI / 180.f;
    float el = elevation_ * (float)M_PI / 180.f;

    float rx = std::cos(az),  rz = -std::sin(az);
    float ux = -std::sin(el) * std::sin(az),
          uy =  std::cos(el),
          uz = -std::sin(el) * std::cos(az);

    float fovRad = fov_ * (float)M_PI / 180.f;
    float scale  = 2.f * distance_ * std::tan(fovRad * 0.5f) / (float)height_;

    target_.x += (double)(-dx * scale * rx  + dy * scale * ux);
    target_.y += (double)(                    dy * scale * uy);
    target_.z += (double)(-dx * scale * rz  + dy * scale * uz);
}
