#ifndef PCLITE_PERSPECTIVE_CAMERA_H
#define PCLITE_PERSPECTIVE_CAMERA_H

#include "camera.h"

// Concrete camera that computes a lookAt view matrix and a perspective
// projection matrix from position_, target_, up_, fov_, near_, far_, aspect_.
// All manipulation is handled by CameraController subclasses.
class PerspectiveCamera : public Camera {
public:
    Mat4f viewMatrix()       const override;
    Mat4f projectionMatrix() const override;
};

#endif // PCLITE_PERSPECTIVE_CAMERA_H
