#ifndef PCLITE_BOUNDARYBOX_H
#define PCLITE_BOUNDARYBOX_H

#include "vec3.h"

template <class T>
class BoundaryBox {
public:
    BoundaryBox(vec3<T> min, vec3<T> max) : min_(min), max_(max) {}

    vec3<T> min() const { return min_; }
    vec3<T> max() const { return max_; }

    void expand(vec3<T> p) {
        if (p.x < min_.x) min_.x = p.x;
        if (p.y < min_.y) min_.y = p.y;
        if (p.z < min_.z) min_.z = p.z;
        if (p.x > max_.x) max_.x = p.x;
        if (p.y > max_.y) max_.y = p.y;
        if (p.z > max_.z) max_.z = p.z;
    }

    // Vector from nearest bbox surface to point (negative inside, positive outside)
    vec3<T> operator-(const vec3<T>& p) const {
        return {
            p.x < min_.x ? min_.x - p.x : (p.x > max_.x ? p.x - max_.x : T(0)),
            p.y < min_.y ? min_.y - p.y : (p.y > max_.y ? p.y - max_.y : T(0)),
            p.z < min_.z ? min_.z - p.z : (p.z > max_.z ? p.z - max_.z : T(0)),
        };
    }

private:
    vec3<T> min_;
    vec3<T> max_;
};

using BoundaryBoxd = BoundaryBox<double>;
using BoundaryBoxf = BoundaryBox<float>;

#endif //PCLITE_BOUNDARYBOX_H
