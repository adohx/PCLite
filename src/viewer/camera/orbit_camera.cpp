#include "orbit_camera.h"
#include <cmath>

// Column-major mat4 (OpenGL convention): m[col*4 + row]
mat4f OrbitCamera::viewMatrix() const {
    float ex = (float)position_.x;
    float ey = (float)position_.y;
    float ez = (float)position_.z;

    // forward = normalize(target - eye)
    float fx = (float)target_.x - ex;
    float fy = (float)target_.y - ey;
    float fz = (float)target_.z - ez;
    float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;

    // right = normalize(cross(forward, world_up))
    float rx = fy * (float)up_.z - fz * (float)up_.y;
    float ry = fz * (float)up_.x - fx * (float)up_.z;
    float rz = fx * (float)up_.y - fy * (float)up_.x;
    float rl = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rl; ry /= rl; rz /= rl;

    // up = cross(right, forward)
    float ux = ry*fz - rz*fy;
    float uy = rz*fx - rx*fz;
    float uz = rx*fy - ry*fx;

    float tx = -(rx*ex + ry*ey + rz*ez);
    float ty = -(ux*ex + uy*ey + uz*ez);
    float tz =  (fx*ex + fy*ey + fz*ez);

    return {
        rx,  ux, -fx, 0.f,
        ry,  uy, -fy, 0.f,
        rz,  uz, -fz, 0.f,
        tx,  ty,  tz, 1.f
    };
}

mat4f OrbitCamera::projectionMatrix() const {
    float f  = 1.f / std::tan(fov_ * (float)M_PI / 360.f);
    float nf = near_ - far_;

    return {
        f / aspect_, 0.f, 0.f,                           0.f,
        0.f,         f,   0.f,                           0.f,
        0.f,         0.f, (far_ + near_) / nf,          -1.f,
        0.f,         0.f, 2.f * far_ * near_ / nf,       0.f
    };
}
