#include "axis_painter.h"
#include "mat.h"
#include <glad/gl.h>
#include <vector>
#include <cmath>

// ── Stick-letter definitions ──────────────────────────────────────────────────
// Each letter is a list of (x0,y0, x1,y1) line segments in a [0,1]×[0,1] box.
struct Seg { float x0,y0,x1,y1; };

static constexpr Seg LETTER_X[] = { {0,0,1,1}, {0,1,1,0} };
static constexpr Seg LETTER_Y[] = { {0,1,.5f,.5f}, {1,1,.5f,.5f}, {.5f,.5f,.5f,0} };
static constexpr Seg LETTER_Z[] = { {0,1,1,1}, {1,1,0,0}, {0,0,1,0} };

template<std::size_t N>
static void drawLetter(const Seg (&segs)[N], float cx, float cy, float scale,
                        float r, float g, float b, std::vector<float>& out) {
    // centre the glyph at (cx, cy)
    float ox = cx - scale * 0.5f;
    float oy = cy - scale * 0.5f;
    for (auto& s : segs) {
        out.insert(out.end(), {ox + s.x0 * scale, oy + s.y0 * scale, 0.f, r, g, b});
        out.insert(out.end(), {ox + s.x1 * scale, oy + s.y1 * scale, 0.f, r, g, b});
    }
}

// Standard GL orthographic projection (math/row-major convention; matches
// how Camera builds its own matrices via operator()(row, col)).
static Mat4f ortho(float l, float r, float b, float t, float n, float f) {
    Mat4f m;
    m(0,0) = 2.f / (r - l);
    m(1,1) = 2.f / (t - b);
    m(2,2) = -2.f / (f - n);
    m(0,3) = -(r + l) / (r - l);
    m(1,3) = -(t + b) / (t - b);
    m(2,3) = -(f + n) / (f - n);
    m(3,3) = 1.f;
    return m;
}

AxisPainter::AxisPainter()
    : shader_(kBasicColorVertexSrc, kBasicColorFragmentSrc) {
    // Axis lines: origin→tip per axis, colored to match the tip/label colors.
    float lineVerts[] = {
        0,0,0, 1.f,0.f,0.f,   1,0,0, 1.f,0.f,0.f,   // X red
        0,0,0, 0.f,1.f,0.f,   0,1,0, 0.f,1.f,0.f,   // Y green
        0,0,0, 0.f,.5f,1.f,   0,0,1, 0.f,.5f,1.f,   // Z blue
    };
    glGenVertexArrays(1, &vaoAxisLines_);
    glBindVertexArray(vaoAxisLines_);
    glGenBuffers(1, &vboAxisLines_);
    glBindBuffer(GL_ARRAY_BUFFER, vboAxisLines_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Axis tip points, same colors as their corresponding line.
    float pointVerts[] = {
        1,0,0, 1.f,0.f,0.f,
        0,1,0, 0.f,1.f,0.f,
        0,0,1, 0.f,.5f,1.f,
    };
    glGenVertexArrays(1, &vaoAxisPoints_);
    glBindVertexArray(vaoAxisPoints_);
    glGenBuffers(1, &vboAxisPoints_);
    glBindBuffer(GL_ARRAY_BUFFER, vboAxisPoints_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pointVerts), pointVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Letter labels: rebuilt every frame (camera-rotation dependent), so just
    // set up the attribute layout now; glBufferData happens in paint().
    glGenVertexArrays(1, &vaoLabels_);
    glBindVertexArray(vaoLabels_);
    glGenBuffers(1, &vboLabels_);
    glBindBuffer(GL_ARRAY_BUFFER, vboLabels_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

AxisPainter::~AxisPainter() {
    glDeleteBuffers(1, &vboAxisLines_);
    glDeleteVertexArrays(1, &vaoAxisLines_);
    glDeleteBuffers(1, &vboAxisPoints_);
    glDeleteVertexArrays(1, &vaoAxisPoints_);
    glDeleteBuffers(1, &vboLabels_);
    glDeleteVertexArrays(1, &vaoLabels_);
}

// ── AxisPainter::paint ────────────────────────────────────────────────────────
void AxisPainter::paint(const Mat4f& viewMatrix, const Mat4f&) {
    // ── Viewport ──────────────────────────────────────────────────────────────
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    constexpr int SIZE   = 100;
    constexpr int MARGIN = 10;
    int x = vp[0] + vp[2] - SIZE - MARGIN;
    int y = vp[1] + MARGIN;

    // ── Save state (no glPushAttrib in core profile) ──────────────────────────
    GLint   prevScissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);
    GLboolean wasScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLfloat prevLineWidth = 1.f;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    GLfloat prevPointSize = 1.f;
    glGetFloatv(GL_POINT_SIZE, &prevPointSize);

    glViewport(x, y, SIZE, SIZE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, SIZE, SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // ── 3D pass: rotation-only view, small ortho box ───────────────────────────
    constexpr float ORTHO = 1.6f;
    Mat4f proj3d = ortho(-ORTHO, ORTHO, -ORTHO, ORTHO, -10.f, 10.f);

    // View: rotation only (zero out translation column)
    Mat4f rot;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            rot(r, c) = viewMatrix(r, c);
    rot(3, 3) = 1.f;

    shader_.use();
    shader_.setMat4("uMVP", proj3d * rot);

    glLineWidth(2.f);
    glBindVertexArray(vaoAxisLines_);
    glDrawArrays(GL_LINES, 0, 6);

    glPointSize(5.f);
    glBindVertexArray(vaoAxisPoints_);
    glDrawArrays(GL_POINTS, 0, 3);

    // ── Labels in 2D screen space ─────────────────────────────────────────────
    // Project axis tips through rotation + ortho → pixel coords inside SIZE×SIZE
    auto project = [&](float wx, float wy, float wz, float& px, float& py) {
        float sx = rot(0,0)*wx + rot(0,1)*wy + rot(0,2)*wz;
        float sy = rot(1,0)*wx + rot(1,1)*wy + rot(1,2)*wz;
        px = (sx / ORTHO + 1.f) * 0.5f * SIZE;
        py = (sy / ORTHO + 1.f) * 0.5f * SIZE;
    };

    float lx, ly;
    constexpr float LABEL_OFFSET = 10.f;   // pixels past the axis tip
    constexpr float GLYPH_SIZE   =  9.f;

    Mat4f proj2d = ortho(0.f, (float)SIZE, 0.f, (float)SIZE, -1.f, 1.f);
    shader_.setMat4("uMVP", proj2d);

    std::vector<float> labelVerts;
    project(1,0,0, lx,ly);
    drawLetter(LETTER_X, lx + LABEL_OFFSET, ly, GLYPH_SIZE, 1.f,0.f,0.f, labelVerts);
    project(0,1,0, lx,ly);
    drawLetter(LETTER_Y, lx, ly + LABEL_OFFSET, GLYPH_SIZE, 0.f,1.f,0.f, labelVerts);
    project(0,0,1, lx,ly);
    drawLetter(LETTER_Z, lx + LABEL_OFFSET, ly, GLYPH_SIZE, 0.f,.5f,1.f, labelVerts);

    glLineWidth(1.5f);
    glBindBuffer(GL_ARRAY_BUFFER, vboLabels_);
    glBufferData(GL_ARRAY_BUFFER, labelVerts.size() * sizeof(float), labelVerts.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(vaoLabels_);
    glDrawArrays(GL_LINES, 0, (GLsizei)(labelVerts.size() / 6));

    glBindVertexArray(0);

    // ── Restore ───────────────────────────────────────────────────────────────
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);
    if (wasScissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (wasDepthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glLineWidth(prevLineWidth);
    glPointSize(prevPointSize);
}
