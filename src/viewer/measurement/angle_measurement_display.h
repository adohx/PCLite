#ifndef PCLITE_ANGLE_MEASUREMENT_DISPLAY_H
#define PCLITE_ANGLE_MEASUREMENT_DISPLAY_H

#include "measurement_display.h"
#include "angle_measurement.h"

// Draws the two rays from the vertex (orange) plus a short connecting arc
// (green, approximated as a handful of straight segments) once all 3 points
// are placed, with one label showing the angle near the vertex. While only
// the vertex + first ray endpoint are known, shows just that partial ray so
// progress is visible mid-click.
class AngleMeasurementDisplay : public MeasurementDisplay {
public:
    explicit AngleMeasurementDisplay(const AngleMeasurement* measurement)
        : measurement_(measurement) {}

    std::vector<MeasureLineSegment> lines() const override;
    std::vector<MeasureLabel> labels() const override;

private:
    const AngleMeasurement* measurement_;
    static constexpr int kArcSegments = 16;
};

#endif //PCLITE_ANGLE_MEASUREMENT_DISPLAY_H
