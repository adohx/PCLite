#include "camera.h"

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
