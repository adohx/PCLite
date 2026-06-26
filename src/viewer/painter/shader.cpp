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
