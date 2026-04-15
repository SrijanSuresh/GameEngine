#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <stdexcept>

namespace Nova {

// Constructor

Shader::Shader(const char* vertSrc, const char* fragSrc) {
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

    m_Program = glCreateProgram();
    glAttachShader(m_Program, vert);
    glAttachShader(m_Program, frag);
    glLinkProgram(m_Program);

    // Check link errors
    GLint success = 0;
    glGetProgramiv(m_Program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_Program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[Shader] Link error:\n%s\n", log);
        glDeleteProgram(m_Program);
        m_Program = 0;
    }

    // Shaders are linked into program, no longer needed
    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (m_Program) {
        glDeleteProgram(m_Program);
        m_Program = 0;
    }
}

// Move semantics

Shader::Shader(Shader&& other) noexcept
    : m_Program(other.m_Program)
{
    other.m_Program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_Program) glDeleteProgram(m_Program);
        m_Program       = other.m_Program;
        other.m_Program = 0;
    }
    return *this;
}

// Bind / Unbind

void Shader::Bind()   const { glUseProgram(m_Program); }
void Shader::Unbind() const { glUseProgram(0); }

// Uniforms

GLint Shader::GetUniformLocation(const char* name) const {
    GLint loc = glGetUniformLocation(m_Program, name);
    if (loc == -1)
        std::fprintf(stderr, "[Shader] Uniform '%s' not found.\n", name);
    return loc;
}

void Shader::SetInt  (const char* name, int value)               const { glUniform1i (GetUniformLocation(name), value); }
void Shader::SetFloat(const char* name, float value)             const { glUniform1f (GetUniformLocation(name), value); }
void Shader::SetVec2 (const char* name, const glm::vec2& value)  const { glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value)); }
void Shader::SetVec3 (const char* name, const glm::vec3& value)  const { glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value)); }
void Shader::SetVec4 (const char* name, const glm::vec4& value)  const { glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value)); }
void Shader::SetMat4 (const char* name, const glm::mat4& value)  const { glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value)); }

// Private: compile single stage

GLuint Shader::CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::fprintf(stderr, "[Shader] %s compile error:\n%s\n", typeName, log);
    }

    return shader;
}

} // namespace Nova
