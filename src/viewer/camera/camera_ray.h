#ifndef PCLITE_CAMERA_RAY_H
#define PCLITE_CAMERA_RAY_H

#include "vec3.h"

// Unit-length direction of the world-space ray from a perspective camera
// through screen pixel (screenX, screenY) of a screenWidth x screenHeight
// viewport (origin top-left, y down -- matches Viewport's mouse coords).
// fovDegrees is the *vertical* field of view, matching Camera::fov().
// Pure math, no GL/Camera dependency, so it's usable (and testable)
// anywhere a camera's position/target/up/fov/aspect are known.
vec3d cameraScreenRayDirection(vec3d position, vec3d target, vec3d up,
                               float fovDegrees, float aspect,
                               float screenX, float screenY,
                               float screenWidth, float screenHeight);

#endif //PCLITE_CAMERA_RAY_H
