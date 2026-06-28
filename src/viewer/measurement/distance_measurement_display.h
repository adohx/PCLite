#ifndef PCLITE_DISTANCE_MEASUREMENT_DISPLAY_H
#define PCLITE_DISTANCE_MEASUREMENT_DISPLAY_H

#include "measurement_display.h"
#include "distance_measurement.h"

// Three lines once both points (and the plane fit) are available:
//   point1-point2 (straight, yellow), point1-foot (perpendicular, cyan),
//   foot-point2 (in-plane, magenta). Degrades to just the straight line if
// the plane fit at point2 failed, or to nothing while only 1 point is placed.
class DistanceMeasurementDisplay : public MeasurementDisplay {
public:
    explicit DistanceMeasurementDisplay(const DistanceMeasurement* measurement)
        : measurement_(measurement) {}

    std::vector<MeasureLineSegment> lines() const override;
    std::vector<MeasureLabel> labels() const override;

private:
    const DistanceMeasurement* measurement_;
};

#endif //PCLITE_DISTANCE_MEASUREMENT_DISPLAY_H
