#ifndef PCLITE_BOUNDING_BOX_H
#define PCLITE_BOUNDING_BOX_H

#include "vec3.h"

template <class T>
class BoundingBox {
public:
    BoundingBox() : min_(), max_() {}
    BoundingBox(vec3<T> min, vec3<T> max) : min_(min), max_(max) ,valid_(true){}

    vec3<T> min()     const { return min_; }
    vec3<T> max()     const { return max_; }
    vec3<T> getSize() const { return {max_.x-min_.x, max_.y-min_.y, max_.z-min_.z}; }

    void expand(vec3<T> p) {
        if (p.x < min_.x) min_.x = p.x;  if (p.x > max_.x) max_.x = p.x;
        if (p.y < min_.y) min_.y = p.y;  if (p.y > max_.y) max_.y = p.y;
        if (p.z < min_.z) min_.z = p.z;  if (p.z > max_.z) max_.z = p.z;
        valid_ = true;
    }

    vec3<T> operator-(const vec3<T>& p) const {
        return {
            p.x < min_.x ? min_.x-p.x : (p.x > max_.x ? p.x-max_.x : T(0)),
            p.y < min_.y ? min_.y-p.y : (p.y > max_.y ? p.y-max_.y : T(0)),
            p.z < min_.z ? min_.z-p.z : (p.z > max_.z ? p.z-max_.z : T(0)),
        };
    }

    bool isValid() const { return valid_; }
private:
    vec3<T> min_, max_;
    bool valid_ = false;
};

using BoundingBoxd = BoundingBox<double>;
using BoundingBoxf = BoundingBox<float>;

#endif //PCLITE_BOUNDING_BOX_H
