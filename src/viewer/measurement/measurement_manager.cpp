#include "measurement_manager.h"
#include "measurement.h"
#include "distance_measurement.h"
#include "distance_measurement_display.h"
#include "angle_measurement.h"
#include "angle_measurement_display.h"

void MeasurementManager::setMode(MeasurementMode mode) {
    mode_ = mode;
    measurement_.reset();
    display_.reset();
}

void MeasurementManager::clear() {
    measurement_.reset();
    display_.reset();
}

void MeasurementManager::startNew() {
    measurement_.reset();
    display_.reset();

    switch (mode_) {
        case MeasurementMode::Distance: {
            auto m = std::make_unique<DistanceMeasurement>();
            display_ = std::make_unique<DistanceMeasurementDisplay>(m.get());
            measurement_ = std::move(m);
            break;
        }
        case MeasurementMode::Angle: {
            auto m = std::make_unique<AngleMeasurement>();
            display_ = std::make_unique<AngleMeasurementDisplay>(m.get());
            measurement_ = std::move(m);
            break;
        }
        case MeasurementMode::None:
            break;
    }
}

void MeasurementManager::onPointPicked(const vec3f& position, bool planeValid,
                                       const vec3f& planeCenter, const vec3f& planeNormal) {
    if (mode_ == MeasurementMode::None) return;
    if (!measurement_ || measurement_->isComplete()) startNew();
    if (measurement_) measurement_->addPoint(position, planeValid, planeCenter, planeNormal);
}

std::vector<MeasureLineSegment> MeasurementManager::currentLines() const {
    return display_ ? display_->lines() : std::vector<MeasureLineSegment>{};
}

std::vector<MeasureLabel> MeasurementManager::currentLabels() const {
    return display_ ? display_->labels() : std::vector<MeasureLabel>{};
}
