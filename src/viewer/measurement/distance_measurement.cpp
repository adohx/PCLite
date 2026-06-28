#include "distance_measurement.h"
#include "measurement_math.h"

void DistanceMeasurement::addPoint(const vec3f& position, bool planeValid,
                                   const vec3f& planeCenter, const vec3f& planeNormal) {
    if (isComplete()) return;

    if (count_ == 0) {
        point1_ = position;
        count_ = 1;
        return;
    }

    point2_ = position;
    count_ = 2;
    pointToPointDistance_ = length(point2_ - point1_);

    hasFoot_ = planeValid;
    if (hasFoot_) computeDerived(planeCenter, planeNormal);
}

void DistanceMeasurement::computeDerived(const vec3f& planeCenter, const vec3f& planeNormal) {
    vec3f n = normalize(planeNormal);
    float signedDist = dot(point1_ - planeCenter, n);
    foot_ = point1_ - n * signedDist;

    perpendicularDistance_ = std::fabs(signedDist);
    inPlaneDistance_ = length(foot_ - point2_);
}
