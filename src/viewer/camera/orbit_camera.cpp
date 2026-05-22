#include "orbit_camera.h"
#include <cmath>

// Rodrigues rotation: rotate v around unit axis k by angle (radians)
static vec3d rodrigues(vec3d v, vec3d k, double rad) {
    double c = std::cos(rad), s = std::sin(rad);
    double dot = k.x*v.x + k.y*v.y + k.z*v.z;
    return {
        v.x*c + (k.y*v.z - k.z*v.y)*s + k.x*dot*(1-c),
        v.y*c + (k.z*v.x - k.x*v.z)*s + k.y*dot*(1-c),
        v.z*c + (k.x*v.y - k.y*v.x)*s + k.z*dot*(1-c),
    };
}

void OrbitCamera::rotate(vec3d axis, float angleDeg) {
    double len = std::sqrt(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    if (len == 0.0) return;
    vec3d k = {axis.x/len, axis.y/len, axis.z/len};
    double rad = angleDeg * M_PI / 180.0;
    vec3d arm = {position_.x - target_.x, position_.y - target_.y, position_.z - target_.z};
    vec3d rotated = rodrigues(arm, k, rad);
    position_ = {target_.x + rotated.x, target_.y + rotated.y, target_.z + rotated.z};
    up_ = rodrigues(up_, k, rad);
}

void OrbitCamera::translate(vec3d delta) {
    position_ = {position_.x + delta.x, position_.y + delta.y, position_.z + delta.z};
    target_   = {target_.x   + delta.x, target_.y   + delta.y, target_.z   + delta.z};
}

void OrbitCamera::zoom(float factor) {
    if (factor <= 0.f) return;
    vec3d arm = {position_.x - target_.x, position_.y - target_.y, position_.z - target_.z};
    position_ = {target_.x + arm.x*factor, target_.y + arm.y*factor, target_.z + arm.z*factor};
}

Mat4f OrbitCamera::viewMatrix() const {
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

    // Row-major LookAt:
    //   row0 = [right,  -dot(right, eye)]
    //   row1 = [up,     -dot(up,    eye)]
    //   row2 = [-fwd,    dot(fwd,   eye)]
    //   row3 = [0, 0, 0, 1]
    Mat4f m;
    m(0,0) =  rx; m(0,1) =  ry; m(0,2) =  rz; m(0,3) = -(rx*ex + ry*ey + rz*ez);
    m(1,0) =  ux; m(1,1) =  uy; m(1,2) =  uz; m(1,3) = -(ux*ex + uy*ey + uz*ez);
    m(2,0) = -fx; m(2,1) = -fy; m(2,2) = -fz; m(2,3) =  (fx*ex + fy*ey + fz*ez);
    m(3,0) = 0.f; m(3,1) = 0.f; m(3,2) = 0.f; m(3,3) = 1.f;
    return m;
}

Mat4f OrbitCamera::projectionMatrix() const {
    float f  = 1.f / std::tan(fov_ * (float)M_PI / 360.f);
    float nf = near_ - far_;

    // Row-major perspective:
    //   row0 = [f/aspect, 0,         0,                       0                   ]
    //   row1 = [0,        f,         0,                       0                   ]
    //   row2 = [0,        0,  (far+near)/nf,   2*far*near/nf                      ]
    //   row3 = [0,        0,        -1,                       0                   ]
    Mat4f m;
    m(0,0) = f / aspect_;
    m(1,1) = f;
    m(2,2) = (far_ + near_) / nf;
    m(2,3) = 2.f * far_ * near_ / nf;
    m(3,2) = -1.f;
    return m;
}
