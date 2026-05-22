#include "camera.h"
#include <cmath>

ProjectionMode Camera::projectionMode() const { return projectionMode_; }
vec3d Camera::position() const { return position_; }
vec3d Camera::target() const { return target_; }
vec3d Camera::up() const { return up_; }
float Camera::fov() const { return fov_; }
float Camera::nearPlane() const { return near_; }
float Camera::farPlane() const { return far_; }
float Camera::aspectRatio() const { return aspect_; }

void Camera::setProjectionMode(ProjectionMode mode) { projectionMode_ = mode; }
void Camera::setPosition(vec3d position) { position_ = position; }
void Camera::setTarget(vec3d target) { target_ = target; }
void Camera::setUp(vec3d up) { up_ = up; }
void Camera::setFov(float fov) { fov_ = fov; }
void Camera::setNearPlane(float near) { near_ = near; }
void Camera::setFarPlane(float far) { far_ = far; }
void Camera::setAspectRatio(float aspect) { aspect_ = aspect; }

void Camera::setManageStrategy(ManageStrategy* strategy) { strategy_ = strategy; }
ManageStrategy* Camera::manageStrategy() const { return strategy_; }

void Camera::lookAt(vec3d point) {
    target_ = point;
}

void Camera::lookAt(const BoundaryBoxd& bbox) {
    // Centre of the bounding box
    vec3d center = {
        (bbox.min().x + bbox.max().x) * 0.5,
        (bbox.min().y + bbox.max().y) * 0.5,
        (bbox.min().z + bbox.max().z) * 0.5,
    };

    // Radius of the bounding sphere that contains the box
    double dx = bbox.max().x - bbox.min().x;
    double dy = bbox.max().y - bbox.min().y;
    double dz = bbox.max().z - bbox.min().z;
    double radius = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.5;

    // Half-angles of vertical and horizontal FOV
    // Horizontal FOV = 2 * atan(tan(verticalFov/2) * aspect)
    double halfFovV = fov_ * M_PI / 360.0;
    double halfFovH = std::atan(std::tan(halfFovV) * aspect_);
    // Use the tighter (smaller) half-angle so the sphere fits in both dimensions
    double halfFov = std::min(halfFovV, halfFovH);

    double dist = radius / std::tan(halfFov);

    // Keep the current viewing direction, only adjust distance
    double armX = position_.x - target_.x;
    double armY = position_.y - target_.y;
    double armZ = position_.z - target_.z;
    double armLen = std::sqrt(armX*armX + armY*armY + armZ*armZ);
    if (armLen > 1e-9) {
        armX /= armLen; armY /= armLen; armZ /= armLen;
    } else {
        armX = 0.0; armY = 0.0; armZ = 1.0;  // fallback
    }

    target_   = center;
    position_ = {center.x + armX * dist,
                 center.y + armY * dist,
                 center.z + armZ * dist};
}
