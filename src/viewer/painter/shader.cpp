#include "shader.h"
#include <glad/gl.h>
#include <stdexcept>
#include <string>
#include <vector>

const char* kBasicColorVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* kBasicColorFragmentSrc = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

const char* kPointCloudVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
uniform int uHighlightIndex; // gl_VertexID to highlight; < 0 disables
out vec3 vColor;
flat out int vHighlighted;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
    vHighlighted = (gl_VertexID == uHighlightIndex) ? 1 : 0;
    gl_PointSize = vHighlighted == 1 ? 8.0 : 2.0;
}
)";

const char* kPointCloudFragmentSrc = R"(#version 330 core
in vec3 vColor;
flat in int vHighlighted;
out vec4 FragColor;
void main() {
    FragColor = vHighlighted == 1 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(vColor, 1.0);
}
)";

const char* kPickVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform uint uNodeId;
flat out uvec2 vId;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vId = uvec2(uNodeId, uint(gl_VertexID));
}
)";

const char* kPickFragmentSrc = R"(#version 330 core
flat in uvec2 vId;
out uvec2 FragId;
void main() {
    FragId = vId;
}
)";

const char* kFlatColorVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* kFlatColorFragmentSrc = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)";

namespace {

unsigned int compile(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed: " + std::string(log.data()));
    }
    return shader;
}

} // namespace

Shader::Shader(const char* vertexSrc, const char* fragmentSrc) {
    unsigned int vs = compile(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetProgramInfoLog(program_, (GLsizei)log.size(), nullptr, log.data());
        glDeleteProgram(program_);
        throw std::runtime_error("Shader linking failed: " + std::string(log.data()));
    }
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

void Shader::use() const {
    glUseProgram(program_);
}

void Shader::setMat4(const char* name, const Mat4f& m) const {
    GLint loc = glGetUniformLocation(program_, name);
    auto arr = m.data(); // column-major, GL convention
    glUniformMatrix4fv(loc, 1, GL_FALSE, arr.data());
}

void Shader::setInt(const char* name, int v) const {
    glUniform1i(glGetUniformLocation(program_, name), v);
}

void Shader::setUint(const char* name, unsigned int v) const {
    glUniform1ui(glGetUniformLocation(program_, name), v);
}

void Shader::setVec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(program_, name), x, y, z, w);
}
