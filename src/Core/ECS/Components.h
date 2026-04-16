#pragma once

// -----------------------------------------------------------------------------
// Components.h
//
// All ECS components used in NovaEngine.
// A component is plain data — no logic. Logic lives in Systems.
//
// How to add a new component:
//   1. Define a struct below
//   2. Attach via registry.emplace<YourComponent>(entity, ...)
//   3. Iterate via registry.view<YourComponent>()
// -----------------------------------------------------------------------------

#include "Core/Renderer/Shader.h"

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <string>
#include <memory>

namespace Nova {

enum class PrimitiveType {
    Triangle,
    Quad
};

// -----------------------------------------------------------------------------
// TagComponent — every entity has a human-readable name
// -----------------------------------------------------------------------------
struct TagComponent {
    std::string Name;

    TagComponent() = default;
    explicit TagComponent(const std::string& name) : Name(name) {}
};

// -----------------------------------------------------------------------------
// TransformComponent — position, rotation (degrees), scale in world space
// -----------------------------------------------------------------------------
struct TransformComponent {
    glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
    glm::vec3 Scale    = { 1.0f, 1.0f, 1.0f };

    TransformComponent() = default;
    explicit TransformComponent(const glm::vec3& position)
        : Position(position) {}
};

// -----------------------------------------------------------------------------
// MeshRendererComponent — GPU handles for drawing this entity
// -----------------------------------------------------------------------------
struct MeshRendererComponent {
    GLuint VAO         = 0;
    GLuint VBO         = 0;
    int    VertexCount = 0;

    MeshRendererComponent() = default;
    MeshRendererComponent(GLuint vao, GLuint vbo, int vertexCount)
        : VAO(vao), VBO(vbo), VertexCount(vertexCount) {}
};

struct PrimitiveComponent {
    PrimitiveType Type = PrimitiveType::Triangle;

    PrimitiveComponent() = default;
    explicit PrimitiveComponent(PrimitiveType type)
        : Type(type) {}
};

struct ColorComponent {
    glm::vec4 Tint = { 1.0f, 1.0f, 1.0f, 1.0f };

    ColorComponent() = default;
    explicit ColorComponent(const glm::vec4& tint)
        : Tint(tint) {}
};

// -----------------------------------------------------------------------------
// MaterialComponent — generative shader material
//
// When the user types a prompt (e.g. "fire galaxy") in the Inspector,
// ShaderGenerator builds a GLSL fragment shader from the keywords.
// That shader is compiled and stored here, replacing the default shader
// for this entity only.
//
// Prompt    : the raw text the user typed
// Mat       : the compiled shader (shared_ptr so it's safe to copy the component)
// IsGenerated : true once a prompt has been successfully compiled
// -----------------------------------------------------------------------------
struct MaterialComponent {
    std::string              Prompt      = "";
    std::shared_ptr<Shader>  Mat         = nullptr;
    bool                     IsGenerated = false;

    MaterialComponent() = default;
};

} // namespace Nova
