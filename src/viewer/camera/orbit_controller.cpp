#include "orbit_controller.h"
#include <algorithm>
#include <cmath>

// SDL button constants (avoid pulling in SDL headers here)
static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

void OrbitController::onMouseDrag(int button, float dx, float dy) {
    if (button == kLeftButton) {
        azimuth_   -= dx * 0.4f;
        elevation_  = std::clamp(elevation_ + dy * 0.4f, -89.f, 89.f);
        return;
    }

    if (button == kRightButton) {
        float az = azimuth_   * (float)M_PI / 180.f;
        float el = elevation_ * (float)M_PI / 180.f;

        // Right and up vectors in world space (derived from view matrix):
        //   right = ( cos(az),               0,          -sin(az)         )
        //   up    = (-sin(el)*sin(az),  cos(el),  -sin(el)*cos(az)        )
        float rx = std::cos(az),  ry = 0.f,              rz = -std::sin(az);
        float ux = -std::sin(el) * std::sin(az),
              uy =  std::cos(el),
              uz = -std::sin(el) * std::cos(az);

        // World units per pixel: derived from vertical FOV and viewport height.
        float fovRad = fov_ * (float)M_PI / 180.f;
        float scale  = 2.f * distance_ * std::tan(fovRad * 0.5f) / (float)height_;

        // Drag right → scene moves right → target moves left (-right direction).
        // Screen Y increases downward, world Y increases upward → +dy maps to +up.
        target_.x += (double)(-dx * scale * rx + dy * scale * ux);
        target_.y += (double)(-dx * scale * ry + dy * scale * uy);
        target_.z += (double)(-dx * scale * rz + dy * scale * uz);
    }
}

void OrbitController::onScroll(float delta) {
    distance_ *= (delta > 0.f) ? 0.85f : 1.18f;
    distance_  = std::clamp(distance_, 0.5f, 5000.f);
}

void OrbitController::onResize(int /*w*/, int h) {
    height_ = h;
}

void OrbitController::syncFromCamera(const Camera& cam) {
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

void OrbitController::applyToCamera(Camera& cam) const {
    float az = azimuth_   * (float)M_PI / 180.f;
    float el = elevation_ * (float)M_PI / 180.f;

    cam.setTarget(target_);
    cam.setPosition({
        target_.x + distance_ * std::cos(el) * std::sin(az),
        target_.y + distance_ * std::sin(el),
        target_.z + distance_ * std::cos(el) * std::cos(az)
    });
}
