#include "camera_ray.h"
#include <cmath>

namespace {
double length(vec3d v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
vec3d normalize(vec3d v) {
    double len = length(v);
    return len > 1e-12 ? vec3d{v.x / len, v.y / len, v.z / len} : v;
}
vec3d cross(vec3d a, vec3d b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
}

vec3d cameraScreenRayDirection(vec3d position, vec3d target, vec3d up,
                               float fovDegrees, float aspect,
                               float screenX, float screenY,
                               float screenWidth, float screenHeight) {
    vec3d fwd   = normalize({target.x - position.x, target.y - position.y, target.z - position.z});
    vec3d rgt   = normalize(cross(fwd, up));
    vec3d camUp = cross(rgt, fwd);

    float ndcX =  (2.f * screenX / screenWidth)  - 1.f;
    float ndcY = -(2.f * screenY / screenHeight) + 1.f; // screen y is top-down, NDC y is up

    float halfFovY = fovDegrees * (float)M_PI / 360.f;
    float tanY = std::tan(halfFovY);
    float tanX = tanY * aspect;

    vec3d dir = {
        fwd.x + rgt.x * (ndcX * tanX) + camUp.x * (ndcY * tanY),
        fwd.y + rgt.y * (ndcX * tanX) + camUp.y * (ndcY * tanY),
        fwd.z + rgt.z * (ndcX * tanX) + camUp.z * (ndcY * tanY),
    };
    return normalize(dir);
}
