//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_VECTOR3_H
#define PCLITE_VECTOR3_H

template <class T>
struct vec3 {
    T x;
    T y;
    T z;

    vec3<T> operator+(const vec3<T> &other) const { return {x + other.x, y + other.y, z + other.z}; }
    vec3<T> operator-(const vec3<T> &other) const { return {x - other.x, y - other.y, z - other.z}; }
    vec3<T> operator*(const T &scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    vec3<T> operator/(const T& scalar) const { return {x / scalar, y / scalar, z / scalar}; }
};

using vec3i = vec3<int>;
using vec3f = vec3<float>;
using vec3d = vec3<double>;

#endif //PCLITE_VECTOR3_H
