#ifndef PCLITE_DISTANCE_MEASUREMENT_H
#define PCLITE_DISTANCE_MEASUREMENT_H

#include "measurement.h"

// Two clicked points, decomposed against a reference plane fitted at the
// SECOND point:
//   point1_ -- the point being measured
//   point2_ -- the reference point; a plane is locally fitted at its
//              neighborhood (reusing the same pick-assist fit as the cyan
//              ring decal -- see Measurement::addPoint)
//   foot_   -- perpendicular projection of point1_ onto that plane
//
// This gives a right triangle with three meaningful lengths:
//   point1_-point2_ : straight-line distance between the two clicked points
//   point1_-foot_   : perpendicular distance from point1_ to the plane
//   foot_-point2_   : in-plane distance from the foot back to point2_
//
// If the plane fit at point2_ failed (e.g. too few neighbors resident), only
// the straight-line distance is meaningful; hasFoot() reports that.
class DistanceMeasurement : public Measurement {
public:
    void addPoint(const vec3f& position, bool planeValid,
                  const vec3f& planeCenter, const vec3f& planeNormal) override;

    int pointCount() const override { return count_; }
    int requiredPointCount() const override { return 2; }

    const vec3f& point1() const { return point1_; }
    const vec3f& point2() const { return point2_; }
    bool hasFoot() const { return hasFoot_; }
    const vec3f& foot() const { return foot_; }

    // Valid once pointCount() >= 2.
    float pointToPointDistance() const { return pointToPointDistance_; }
    // Valid once hasFoot().
    float perpendicularDistance() const { return perpendicularDistance_; }
    float inPlaneDistance() const { return inPlaneDistance_; }

private:
    int count_ = 0;
    vec3f point1_{}, point2_{};
    bool hasFoot_ = false;
    vec3f foot_{};
    float pointToPointDistance_ = 0.f;
    float perpendicularDistance_ = 0.f;
    float inPlaneDistance_ = 0.f;

    void computeDerived(const vec3f& planeCenter, const vec3f& planeNormal);
};

#endif //PCLITE_DISTANCE_MEASUREMENT_H
