#include "distance_measurement_display.h"
#include "measurement_math.h"
#include <cstdio>

std::vector<MeasureLineSegment> DistanceMeasurementDisplay::lines() const {
    std::vector<MeasureLineSegment> out;
    if (measurement_->pointCount() < 2) return out;

    out.push_back({measurement_->point1(), measurement_->point2(), 1.f, 1.f, 0.f, 1.f}); // yellow
    if (measurement_->hasFoot()) {
        out.push_back({measurement_->point1(), measurement_->foot(), 0.f, 1.f, 1.f, 1.f}); // cyan
        out.push_back({measurement_->foot(), measurement_->point2(), 1.f, 0.f, 1.f, 1.f}); // magenta
    }
    return out;
}

std::vector<MeasureLabel> DistanceMeasurementDisplay::labels() const {
    std::vector<MeasureLabel> out;
    if (measurement_->pointCount() < 2) return out;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Dist: %.3f m", measurement_->pointToPointDistance());
    out.push_back({midpoint(measurement_->point1(), measurement_->point2()), buf});

    if (measurement_->hasFoot()) {
        std::snprintf(buf, sizeof(buf), "Perp: %.3f m", measurement_->perpendicularDistance());
        out.push_back({midpoint(measurement_->point1(), measurement_->foot()), buf});

        std::snprintf(buf, sizeof(buf), "InPlane: %.3f m", measurement_->inPlaneDistance());
        out.push_back({midpoint(measurement_->foot(), measurement_->point2()), buf});
    }
    return out;
}
