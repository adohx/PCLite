#include "perspective_camera.h"
#include <cmath>

Mat4f PerspectiveCamera::viewMatrix() const {
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
    float rx = fy*(float)up_.z - fz*(float)up_.y;
    float ry = fz*(float)up_.x - fx*(float)up_.z;
    float rz = fx*(float)up_.y - fy*(float)up_.x;
    float rl = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rl; ry /= rl; rz /= rl;

    // up = cross(right, forward)
    float ux = ry*fz - rz*fy;
    float uy = rz*fx - rx*fz;
    float uz = rx*fy - ry*fx;

    Mat4f m;
    m(0,0) =  rx; m(0,1) =  ry; m(0,2) =  rz; m(0,3) = -(rx*ex + ry*ey + rz*ez);
    m(1,0) =  ux; m(1,1) =  uy; m(1,2) =  uz; m(1,3) = -(ux*ex + uy*ey + uz*ez);
    m(2,0) = -fx; m(2,1) = -fy; m(2,2) = -fz; m(2,3) =  (fx*ex + fy*ey + fz*ez);
    m(3,0) = 0.f; m(3,1) = 0.f; m(3,2) = 0.f; m(3,3) = 1.f;
    return m;
}

Mat4f PerspectiveCamera::projectionMatrix() const {
    float f  = 1.f / std::tan(fov_ * (float)M_PI / 360.f);
    float nf = near_ - far_;

    Mat4f m;
    m(0,0) = f / aspect_;
    m(1,1) = f;
    m(2,2) = (far_ + near_) / nf;
    m(2,3) = 2.f * far_ * near_ / nf;
    m(3,2) = -1.f;
    return m;
}
