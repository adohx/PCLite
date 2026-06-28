#include "angle_measurement_display.h"
#include "measurement_math.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr float kOrange[3] = {1.f, 0.6f, 0.f};
constexpr float kGreen[3]  = {0.f, 1.f, 0.f};
}

std::vector<MeasureLineSegment> AngleMeasurementDisplay::lines() const {
    std::vector<MeasureLineSegment> out;
    int n = measurement_->pointCount();
    if (n < 2) return out; // no vertex yet, nothing meaningful to draw

    const vec3f& vertex = measurement_->vertex();
    out.push_back({vertex, measurement_->rayStart(), kOrange[0], kOrange[1], kOrange[2], 1.f});
    if (n < 3) return out;

    const vec3f& rayEnd = measurement_->rayEnd();
    out.push_back({vertex, rayEnd, kOrange[0], kOrange[1], kOrange[2], 1.f});

    vec3f a = normalize(measurement_->rayStart() - vertex);
    vec3f b = normalize(rayEnd - vertex);
    float c = std::max(-1.f, std::min(1.f, dot(a, b)));
    float theta = std::acos(c);

    vec3f w = b - a * dot(a, b);
    float wLen = length(w);
    if (wLen < 1e-6f || theta < 1e-4f) return out; // rays (anti)parallel: no well-defined arc plane
    w = w * (1.f / wLen);

    float radius = 0.2f * std::min(length(measurement_->rayStart() - vertex), length(rayEnd - vertex));
    radius = std::max(radius, 1e-4f);

    vec3f prev = vertex + a * radius;
    for (int i = 1; i <= kArcSegments; ++i) {
        float t = theta * (float)i / (float)kArcSegments;
        vec3f dir = a * std::cos(t) + w * std::sin(t);
        vec3f cur = vertex + dir * radius;
        out.push_back({prev, cur, kGreen[0], kGreen[1], kGreen[2], 1.f});
        prev = cur;
    }
    return out;
}

std::vector<MeasureLabel> AngleMeasurementDisplay::labels() const {
    std::vector<MeasureLabel> out;
    if (measurement_->pointCount() < 3) return out;

    const vec3f& vertex = measurement_->vertex();
    vec3f a = normalize(measurement_->rayStart() - vertex);
    vec3f b = normalize(measurement_->rayEnd() - vertex);
    vec3f bisector = a + b;
    float bisLen = length(bisector);
    vec3f dir = bisLen > 1e-6f ? bisector * (1.f / bisLen) : a;

    float radius = 0.2f * std::min(length(measurement_->rayStart() - vertex),
                                    length(measurement_->rayEnd() - vertex));
    radius = std::max(radius, 1e-4f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "Angle: %.2f deg", measurement_->angleDegrees());
    out.push_back({vertex + dir * (radius * 1.3f), buf});
    return out;
}
