//
// Created by cj on 2026-05-14.
//

#ifndef PCLITE_BOUNDARYBOX_H
#define PCLITE_BOUNDARYBOX_H
#include <memory>

#include "vec3.h"


template <class T>
class BoundaryBox {
public:
    explicit BoundaryBox(vec3<T> min, vec3<T> max);
    virtual ~BoundaryBox();

    void expand(vec3<T> point);
    vec3<T> min() const;
    vec3<T> max() const;
    vec3<T> operator-(const vec3<T> &point) const;


private:
    class BoundaryBoxPrivate;
    std::unique_ptr<BoundaryBox> d_;
};

using BoundaryBoxd=BoundaryBox<double>;

#endif //PCLITE_BOUNDARYBOX_H
