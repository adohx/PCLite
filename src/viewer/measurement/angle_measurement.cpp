#include "angle_measurement.h"
#include "measurement_math.h"
#include <algorithm>

void AngleMeasurement::addPoint(const vec3f& position, bool /*planeValid*/,
                                const vec3f& /*planeCenter*/, const vec3f& /*planeNormal*/) {
    if (isComplete()) return;

    if (count_ == 0) {
        rayStart_ = position;
    } else if (count_ == 1) {
        vertex_ = position;
    } else {
        rayEnd_ = position;
    }
    ++count_;

    if (count_ == 3) computeAngle();
}

void AngleMeasurement::computeAngle() {
    vec3f a = normalize(rayStart_ - vertex_);
    vec3f b = normalize(rayEnd_ - vertex_);
    float c = dot(a, b);
    c = std::max(-1.f, std::min(1.f, c));
    angleDegrees_ = std::acos(c) * 180.f / 3.14159265358979323846f;
}
