#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>

namespace Nova {

class Shader {
public:
    Shader() = default;
    Shader(const char* vertSrc, const char* fragSrc);
    ~Shader();

    // Non-copyable, movable
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void Bind()   const;
    void Unbind() const;

    // Uniforms
    void SetInt  (const char* name, int value)              const;
    void SetFloat(const char* name, float value)            const;
    void SetVec2 (const char* name, const glm::vec2& value) const;
    void SetVec3 (const char* name, const glm::vec3& value) const;
    void SetVec4 (const char* name, const glm::vec4& value) const;
    void SetMat4 (const char* name, const glm::mat4& value) const;

    bool IsValid() const { return m_Program != 0; }

private:
    static GLuint CompileShader(GLenum type, const char* src);
    GLint  GetUniformLocation(const char* name) const;

    GLuint m_Program = 0;
};

} // namespace Nova