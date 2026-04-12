#pragma once

// -----------------------------------------------------------------------------
// Components.h
//
// This file defines all ECS components used in NovaEngine.
// A "component" is just a plain data struct — it holds information but has
// no logic. Logic lives in Systems (see Systems/).
//
// How to add a new component:
//   1. Define a struct below
//   2. Add it to an entity via scene.GetRegistry().emplace<YourComponent>(entity)
//   3. Iterate it in a System using registry.view<YourComponent>()
// -----------------------------------------------------------------------------

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <string>

namespace Nova {

// -----------------------------------------------------------------------------
// TagComponent
// Every entity has a name. This is how we identify entities in the Hierarchy.
// -----------------------------------------------------------------------------
struct TagComponent {
    std::string Name;

    TagComponent() = default;
    explicit TagComponent(const std::string& name) : Name(name) {}
};

// -----------------------------------------------------------------------------
// TransformComponent
// Where an entity lives in the world: position, rotation (degrees), and scale.
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
// MeshRendererComponent
// Tells the render system how to draw this entity.
// Holds the GPU-side handles (VAO) and how many vertices to draw.
//
// Note: We store a raw shader pointer here for simplicity.
//       Later this will become a material/asset handle.
// -----------------------------------------------------------------------------
struct MeshRendererComponent {
    GLuint VAO         = 0;  // Vertex Array Object — describes vertex layout
    GLuint VBO         = 0;  // Vertex Buffer Object — the actual vertex data
    int    VertexCount = 0;  // How many vertices to draw

    MeshRendererComponent() = default;
    MeshRendererComponent(GLuint vao, GLuint vbo, int vertexCount)
        : VAO(vao), VBO(vbo), VertexCount(vertexCount) {}
};

} // namespace Nova