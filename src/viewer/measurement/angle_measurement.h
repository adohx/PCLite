#ifndef PCLITE_ANGLE_MEASUREMENT_H
#define PCLITE_ANGLE_MEASUREMENT_H

#include "measurement.h"

// Three clicked points, in order: rayStart_ (1st click), vertex_ (2nd
// click), rayEnd_ (3rd click). The measured angle is the angle at vertex_
// between the rays to rayStart_ and rayEnd_. Plane-fit info passed to
// addPoint() is unused -- an angle is pure 3D geometry, no surface needed.
class AngleMeasurement : public Measurement {
public:
    void addPoint(const vec3f& position, bool planeValid,
                  const vec3f& planeCenter, const vec3f& planeNormal) override;

    int pointCount() const override { return count_; }
    int requiredPointCount() const override { return 3; }

    // Only as many of these as pointCount() indicates are meaningful.
    const vec3f& rayStart() const { return rayStart_; }
    const vec3f& vertex() const { return vertex_; }
    const vec3f& rayEnd() const { return rayEnd_; }

    // Valid once pointCount() >= 3.
    float angleDegrees() const { return angleDegrees_; }

private:
    int count_ = 0;
    vec3f rayStart_{}, vertex_{}, rayEnd_{};
    float angleDegrees_ = 0.f;

    void computeAngle();
};

#endif //PCLITE_ANGLE_MEASUREMENT_H
