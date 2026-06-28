#include "measurement_painter.h"
#include "measurement/measurement_manager.h"
#include <glad/gl.h>

MeasurementPainter::MeasurementPainter(const MeasurementManager* manager)
    : manager_(manager), shader_(kFlatColorVertexSrc, kFlatColorFragmentSrc) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

MeasurementPainter::~MeasurementPainter() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void MeasurementPainter::paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    if (!manager_) return;
    auto segments = manager_->currentLines();
    if (segments.empty()) return;

    GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLfloat prevLineWidth = 1.f;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);

    glDisable(GL_DEPTH_TEST); // always visible on top, same precedent as PlaneFitRingPainter
    glLineWidth(2.f);

    shader_.use();
    shader_.setMat4("uMVP", projMatrix * viewMatrix);
    glBindVertexArray(vao_);

    for (auto& seg : segments) {
        float verts[6] = {seg.a.x, seg.a.y, seg.a.z, seg.b.x, seg.b.y, seg.b.z};
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        shader_.setVec4("uColor", seg.r, seg.g, seg.b_, seg.alpha);
        glDrawArrays(GL_LINES, 0, 2);
    }

    glBindVertexArray(0);
    glLineWidth(prevLineWidth);
    if (wasDepthTestEnabled) glEnable(GL_DEPTH_TEST);
}
