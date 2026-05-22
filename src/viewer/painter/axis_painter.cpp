#include "axis_painter.h"
#include "mat.h"
#include <SDL2/SDL_opengl.h>
#include <cmath>

// ── Stick-letter definitions ──────────────────────────────────────────────────
// Each letter is a list of (x0,y0, x1,y1) line segments in a [0,1]×[0,1] box.
struct Seg { float x0,y0,x1,y1; };

static constexpr Seg LETTER_X[] = { {0,0,1,1}, {0,1,1,0} };
static constexpr Seg LETTER_Y[] = { {0,1,.5f,.5f}, {1,1,.5f,.5f}, {.5f,.5f,.5f,0} };
static constexpr Seg LETTER_Z[] = { {0,1,1,1}, {1,1,0,0}, {0,0,1,0} };

template<std::size_t N>
static void drawLetter(const Seg (&segs)[N], float cx, float cy, float scale) {
    // centre the glyph at (cx, cy)
    float ox = cx - scale * 0.5f;
    float oy = cy - scale * 0.5f;
    for (auto& s : segs) {
        glVertex2f(ox + s.x0 * scale, oy + s.y0 * scale);
        glVertex2f(ox + s.x1 * scale, oy + s.y1 * scale);
    }
}

// ── AxisPainter::paint ────────────────────────────────────────────────────────
void AxisPainter::paint(const Mat4f& viewMatrix) {
    // ── Viewport ──────────────────────────────────────────────────────────────
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    constexpr int SIZE   = 100;
    constexpr int MARGIN = 10;
    int x = vp[0] + vp[2] - SIZE - MARGIN;
    int y = vp[1] + MARGIN;

    // ── Save state ────────────────────────────────────────────────────────────
    glPushAttrib(GL_VIEWPORT_BIT | GL_SCISSOR_BIT | GL_ENABLE_BIT |
                 GL_LINE_BIT | GL_POINT_BIT | GL_CURRENT_BIT);

    glViewport(x, y, SIZE, SIZE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, SIZE, SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // ── Projection ────────────────────────────────────────────────────────────
    constexpr float ORTHO = 1.6f;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-ORTHO, ORTHO, -ORTHO, ORTHO, -10.0, 10.0);

    // ── View: rotation only (zero out translation column) ─────────────────────
    Mat4f rot;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            rot(r, c) = viewMatrix(r, c);
    rot(3, 3) = 1.f;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    auto rotArr = rot.data();
    glLoadMatrixf(rotArr.data());

    // ── Axes ──────────────────────────────────────────────────────────────────
    glLineWidth(2.f);
    glBegin(GL_LINES);
        glColor3f(1.f, 0.f, 0.f); glVertex3f(0,0,0); glVertex3f(1,0,0);
        glColor3f(0.f, 1.f, 0.f); glVertex3f(0,0,0); glVertex3f(0,1,0);
        glColor3f(0.f,.5f, 1.f);  glVertex3f(0,0,0); glVertex3f(0,0,1);
    glEnd();

    glPointSize(5.f);
    glBegin(GL_POINTS);
        glColor3f(1.f, 0.f, 0.f); glVertex3f(1,0,0);
        glColor3f(0.f, 1.f, 0.f); glVertex3f(0,1,0);
        glColor3f(0.f,.5f, 1.f);  glVertex3f(0,0,1);
    glEnd();

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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SIZE, 0, SIZE, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glLineWidth(1.5f);
    glBegin(GL_LINES);
        // X label
        project(1,0,0, lx,ly);
        glColor3f(1.f, 0.f, 0.f);
        drawLetter(LETTER_X, lx + LABEL_OFFSET, ly, GLYPH_SIZE);
        // Y label
        project(0,1,0, lx,ly);
        glColor3f(0.f, 1.f, 0.f);
        drawLetter(LETTER_Y, lx, ly + LABEL_OFFSET, GLYPH_SIZE);
        // Z label
        project(0,0,1, lx,ly);
        glColor3f(0.f,.5f, 1.f);
        drawLetter(LETTER_Z, lx + LABEL_OFFSET, ly, GLYPH_SIZE);
    glEnd();

    // ── Restore ───────────────────────────────────────────────────────────────
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    glEnable(GL_DEPTH_TEST);
}
