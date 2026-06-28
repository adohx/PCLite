#ifndef PCLITE_MEASUREMENT_MANAGER_H
#define PCLITE_MEASUREMENT_MANAGER_H

#include "measurement.h"
#include "measurement_display.h"
#include "vec3.h"
#include <memory>

enum class MeasurementMode {
    None,
    Distance,
    Angle,
};

// Owns at most one "current" measurement (plus its paired display) at a
// time. Two jobs, per the design this mirrors:
//   1. Forward resolved point picks (mouse clicks, already resolved to a 3D
//      position by Viewport's existing pick mechanism) to the current
//      Measurement.
//   2. Handle swapping the current measurement object out -- either
//      explicitly on setMode()/clear(), or implicitly: a click that arrives
//      after the current measurement is already complete starts a fresh
//      one of the same type rather than being appended to the finished one.
class MeasurementManager {
public:
    void setMode(MeasurementMode mode);
    MeasurementMode mode() const { return mode_; }

    // Discards the current (in-progress or completed) measurement without
    // changing mode_; the next point picked starts a fresh one.
    void clear();

    void onPointPicked(const vec3f& position, bool planeValid,
                       const vec3f& planeCenter, const vec3f& planeNormal);

    std::vector<MeasureLineSegment> currentLines() const;
    std::vector<MeasureLabel> currentLabels() const;

private:
    MeasurementMode mode_ = MeasurementMode::None;
    std::unique_ptr<Measurement> measurement_;
    std::unique_ptr<MeasurementDisplay> display_;

    void startNew();
};

#endif //PCLITE_MEASUREMENT_MANAGER_H
