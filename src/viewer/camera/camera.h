#ifndef PCLITE_CAMERA_H
#define PCLITE_CAMERA_H

#include "vec3.h"
#include "mat.h"
#include "bounding_box.h"

class ManageStrategy;

enum class ProjectionMode {
    Perspective,
    Orthographic,
};

class Camera {
public:
    virtual ~Camera() = default;

    virtual Mat4f viewMatrix()       const = 0;
    virtual Mat4f projectionMatrix() const = 0;

    // Camera manipulation — subclasses define the concrete behaviour.
    virtual void rotate(vec3d axis, float angleDeg) = 0;
    virtual void translate(vec3d delta)             = 0;
    virtual void zoom(float factor)                 = 0;

    // Point the camera at a target point or the centre of a bounding box.
    void lookAt(vec3d point);
    void lookAt(const BoundingBoxd& bbox);

    ProjectionMode projectionMode() const;
    vec3d position() const;
    vec3d target() const;
    vec3d up() const;
    float fov() const;
    float nearPlane() const;
    float farPlane() const;
    float aspectRatio() const;

    void setProjectionMode(ProjectionMode mode);
    void setPosition(vec3d position);
    void setTarget(vec3d target);
    void setUp(vec3d up);
    void setFov(float fov);
    void setNearPlane(float near);
    void setFarPlane(float far);
    void setAspectRatio(float aspect);

    void setManageStrategy(ManageStrategy* strategy);
    ManageStrategy* manageStrategy() const;

protected:
    ProjectionMode projectionMode_ = ProjectionMode::Perspective;
    vec3d position_ = {0.0, 0.0, 10.0};
    vec3d target_   = {0.0, 0.0,  0.0};
    vec3d up_       = {0.0, 1.0,  0.0};
    float fov_      = 60.0f;
    float near_     = 0.1f;
    float far_      = 1000.0f;
    float aspect_   = 1.0f;

    ManageStrategy* strategy_ = nullptr;
};

#endif //PCLITE_CAMERA_H
