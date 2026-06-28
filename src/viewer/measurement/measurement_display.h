#ifndef PCLITE_MEASUREMENT_DISPLAY_H
#define PCLITE_MEASUREMENT_DISPLAY_H

#include "vec3.h"
#include <string>
#include <vector>

// How to draw one Measurement: turns its (possibly still in-progress) state
// into renderable geometry + label text. Kept separate from Measurement
// itself so the same measurement logic could grow alternate presentations
// later without touching the geometry/value computation.
//
// A concrete display is paired 1:1 with a concrete Measurement subclass at
// construction (it holds a typed pointer to it, not the abstract
// Measurement base) since interpreting e.g. a DistanceMeasurement's fields
// as line segments is type-specific.

struct MeasureLineSegment {
    vec3f a, b;
    float r = 1.f, g = 1.f, b_ = 1.f, alpha = 1.f;
};

struct MeasureLabel {
    vec3f anchor;
    std::string text;
};

class MeasurementDisplay {
public:
    virtual ~MeasurementDisplay() = default;
    virtual std::vector<MeasureLineSegment> lines() const = 0;
    virtual std::vector<MeasureLabel> labels() const = 0;
};

#endif //PCLITE_MEASUREMENT_DISPLAY_H
