#include "rotation_center_painter.h"
#include "camera/camera.h"
#include <glad/gl.h>
#include <cmath>

RotationCenterPainter::RotationCenterPainter()
    : shader_(kBasicColorVertexSrc, kBasicColorFragmentSrc) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

RotationCenterPainter::~RotationCenterPainter() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void RotationCenterPainter::syncCamera(const Camera& camera) {
    target_ = camera.target();

    vec3d pos = camera.position();
    double dx = pos.x - target_.x, dy = pos.y - target_.y, dz = pos.z - target_.z;
    cameraDistance_ = (float)std::sqrt(dx * dx + dy * dy + dz * dz);

    rebuildGeometry();
}

void RotationCenterPainter::rebuildGeometry() {
    // Scale with distance to camera so the crosshair reads as a constant
    // screen size whether the user is zoomed in or far out.
    float arm = std::max(cameraDistance_ * 0.06f, 1e-4f);

    float cx = (float)target_.x, cy = (float)target_.y, cz = (float)target_.z;
    constexpr float r = 1.f, g = 0.f, b = 1.f; // magenta: contrasts with typical white/grey/brown scans

    float verts[] = {
        cx - arm, cy, cz, r, g, b,  cx + arm, cy, cz, r, g, b,
        cx, cy - arm, cz, r, g, b,  cx, cy + arm, cz, r, g, b,
        cx, cy, cz - arm, r, g, b,  cx, cy, cz + arm, r, g, b,
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
}

void RotationCenterPainter::paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLfloat prevLineWidth = 1.f;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);

    // Always render on top so the pivot point stays visible even when it's
    // behind the point cloud from the current view.
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.f);

    shader_.use();
    shader_.setMat4("uMVP", projMatrix * viewMatrix);

    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, 6);
    glBindVertexArray(0);

    if (wasDepthTestEnabled) glEnable(GL_DEPTH_TEST);
    glLineWidth(prevLineWidth);
}
