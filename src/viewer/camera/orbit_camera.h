#ifndef PCLITE_ORBIT_CAMERA_H
#define PCLITE_ORBIT_CAMERA_H

#include "camera.h"

// Concrete camera that computes view/projection from position_, target_, up_, fov_, etc.
// The calling code (e.g. SDLWindow) is responsible for updating position_ each frame
// based on orbit state (azimuth, elevation, distance).
class OrbitCamera : public Camera {
public:
    Mat4f viewMatrix()       const override;
    Mat4f projectionMatrix() const override;

    // Rotate position_ around target_ along the given world-space axis.
    void rotate(vec3d axis, float angleDeg) override;
    // Shift both position_ and target_ by delta.
    void translate(vec3d delta) override;
    // Scale the distance from position_ to target_ by factor.
    void zoom(float factor) override;
};

#endif //PCLITE_ORBIT_CAMERA_H
