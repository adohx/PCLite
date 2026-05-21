#ifndef PCLITE_ORBIT_CAMERA_H
#define PCLITE_ORBIT_CAMERA_H

#include "camera.h"

// Concrete camera that computes view/projection from position_, target_, up_, fov_, etc.
// The calling code (e.g. SDLWindow) is responsible for updating position_ each frame
// based on orbit state (azimuth, elevation, distance).
class OrbitCamera : public Camera {
public:
    mat4f viewMatrix()       const override;
    mat4f projectionMatrix() const override;
};

#endif //PCLITE_ORBIT_CAMERA_H
