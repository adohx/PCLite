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

private:
    unsigned int program_ = 0;
};

// All painters draw plain (position, color) vertices through the same
// uMVP-transformed pipeline, so they share one GLSL source pair instead of
// each embedding a copy of the same ten lines.
extern const char* kBasicColorVertexSrc;
extern const char* kBasicColorFragmentSrc;

#endif // PCLITE_SHADER_H
