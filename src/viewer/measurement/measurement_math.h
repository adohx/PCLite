#ifndef PCLITE_MEASUREMENT_MATH_H
#define PCLITE_MEASUREMENT_MATH_H

#include "vec3.h"
#include <cmath>

// Small vec3f helpers shared by the measurement classes and their displays.
// (vec3<T> itself only defines +,-,* scalar,/ scalar -- see core/vec3.h.)

inline float dot(const vec3f& a, const vec3f& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length(const vec3f& v) {
    return std::sqrt(dot(v, v));
}

inline vec3f normalize(const vec3f& v) {
    float len = length(v);
    return len > 1e-12f ? v * (1.f / len) : v;
}

inline vec3f cross(const vec3f& a, const vec3f& b) {
    return vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline vec3f midpoint(const vec3f& a, const vec3f& b) {
    return (a + b) * 0.5f;
}

#endif //PCLITE_MEASUREMENT_MATH_H
