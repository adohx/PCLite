#ifndef PCLITE_MEASUREMENT_H
#define PCLITE_MEASUREMENT_H

#include "vec3.h"

// Base class for one in-progress-or-complete measurement (e.g. distance,
// angle): pure geometry/state, no GL and no notion of color/text -- that's
// MeasurementDisplay's job (see measurement_display.h). A concrete
// Measurement only knows how to accumulate the points it's clicked and
// compute its own derived quantities (lengths, angle, ...).
//
// addPoint() is called once per resolved point pick, in click order, for
// every measurement type uniformly -- planeValid/planeCenter/planeNormal
// carry the locally-fitted-plane info Viewport's existing pick-assist
// mechanism (the same KD-tree fit behind the cyan ring decal) already
// computed for that exact click; only DistanceMeasurement uses it, other
// types just ignore the extra arguments.
class Measurement {
public:
    virtual ~Measurement() = default;

    virtual void addPoint(const vec3f& position, bool planeValid,
                          const vec3f& planeCenter, const vec3f& planeNormal) = 0;

    virtual int pointCount() const = 0;
    virtual int requiredPointCount() const = 0;
    bool isComplete() const { return pointCount() >= requiredPointCount(); }
};

#endif //PCLITE_MEASUREMENT_H
