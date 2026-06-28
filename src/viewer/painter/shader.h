#ifndef PCLITE_SHADER_H
#define PCLITE_SHADER_H

#include "mat.h"

// Minimal GLSL program wrapper: compiles+links a vertex/fragment pair and
// uploads a Mat4f uniform. Throws std::runtime_error (with the compile/link
// log) on failure. Non-copyable; each painter owns exactly one as a plain
// member, never reassigned, so no move support is needed either.
class Shader {
public:
    Shader(const char* vertexSrc, const char* fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setMat4(const char* name, const Mat4f& m) const;
    void setInt(const char* name, int v) const;
    void setUint(const char* name, unsigned int v) const;
    void setVec4(const char* name, float x, float y, float z, float w) const;
    void setFloat(const char* name, float v) const;

private:
    unsigned int program_ = 0;
};

// All painters draw plain (position, color) vertices through the same
// uMVP-transformed pipeline, so they share one GLSL source pair instead of
// each embedding a copy of the same ten lines.
extern const char* kBasicColorVertexSrc;
extern const char* kBasicColorFragmentSrc;

// NodePainter's point-cloud shader: same (position, color) pipeline as
// above, plus a single-point highlight override selected by gl_VertexID
// (set uHighlightIndex < 0 to disable). Kept separate from the shared basic
// shader so other painters (axis, bbox, rotation center) aren't coupled to
// picking/highlight state they don't use.
extern const char* kPointCloudVertexSrc;
extern const char* kPointCloudFragmentSrc;

// Pick pass: renders gl_VertexID and a per-draw-call node id into an
// integer render target instead of color, so a point can be identified by
// reading back one pixel. Position-only attribute (location 0); the
// color attribute (location 1) bound in the same VAO is simply unused.
extern const char* kPickVertexSrc;
extern const char* kPickFragmentSrc;

// Flat, single-color translucent fill: position-only attribute, one uColor
// (rgba) uniform for the whole draw call. Used by PlaneFitRingPainter's
// annulus, which has no per-vertex color/alpha to carry.
extern const char* kFlatColorVertexSrc;
extern const char* kFlatColorFragmentSrc;

#endif // PCLITE_SHADER_H
