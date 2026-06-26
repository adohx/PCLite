#include "plane_fit_ring_painter.h"
#include "camera/camera.h"
#include <glad/gl.h>
#include <cmath>

PlaneFitRingPainter::PlaneFitRingPainter()
    : shader_(kFlatColorVertexSrc, kFlatColorFragmentSrc) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

PlaneFitRingPainter::~PlaneFitRingPainter() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void PlaneFitRingPainter::setPlaneFit(bool active, const vec3f& center, const vec3f& normal, float /*radius*/) {
    active_ = active;
    if (!active_) return;

    center_ = center;
    normal_ = normal;
    rebuildGeometry();
}

void PlaneFitRingPainter::syncCamera(const Camera& camera) {
    if (!active_) return;

    vec3d pos = camera.position();
    double dx = pos.x - center_.x, dy = pos.y - center_.y, dz = pos.z - center_.z;
    cameraDistance_ = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    rebuildGeometry();
}

void PlaneFitRingPainter::rebuildGeometry() {
    float outerR = std::max(cameraDistance_ * kOuterFraction, 1e-4f);
    float innerR = outerR * kInnerFraction;

    // Any vector not parallel to normal_ works as a seed; pick whichever
    // world axis is least aligned with it to avoid a near-zero cross product.
    vec3f seed = (std::fabs(normal_.z) < 0.9f) ? vec3f{0, 0, 1} : vec3f{1, 0, 0};

    auto cross = [](const vec3f& a, const vec3f& b) {
        return vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    };
    auto normalize = [](vec3f v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return len > 1e-12f ? vec3f{v.x / len, v.y / len, v.z / len} : v;
    };

    vec3f u = normalize(cross(normal_, seed));
    vec3f v = normalize(cross(normal_, u));

    // Triangle strip alternating outer/inner around the ring: outer[0],
    // inner[0], outer[1], inner[1], ..., wrapping back to segment 0 so the
    // band closes with no seam. The disc inside innerR is simply never
    // covered by any triangle, so it stays fully transparent.
    std::vector<float> verts;
    verts.reserve((kSegments + 1) * 2 * 3);
    for (int i = 0; i <= kSegments; ++i) {
        float theta = 2.f * 3.14159265f * static_cast<float>(i) / static_cast<float>(kSegments);
        float c = std::cos(theta), s = std::sin(theta);
        vec3f dir{c * u.x + s * v.x, c * u.y + s * v.y, c * u.z + s * v.z};

        vec3f outer{center_.x + outerR * dir.x, center_.y + outerR * dir.y, center_.z + outerR * dir.z};
        vec3f inner{center_.x + innerR * dir.x, center_.y + innerR * dir.y, center_.z + innerR * dir.z};

        verts.push_back(outer.x); verts.push_back(outer.y); verts.push_back(outer.z);
        verts.push_back(inner.x); verts.push_back(inner.y); verts.push_back(inner.z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
}

void PlaneFitRingPainter::paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    if (!active_) return;

    GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);

    // Always render on top (same precedent as RotationCenterPainter) and
    // alpha-blend the translucent band over whatever's behind it.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setMat4("uMVP", projMatrix * viewMatrix);
    shader_.setVec4("uColor", 0.f, 1.f, 1.f, 0.5f); // cyan, semi-transparent

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (kSegments + 1) * 2);
    glBindVertexArray(0);

    if (wasDepthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (!wasBlendEnabled) glDisable(GL_BLEND);
}
