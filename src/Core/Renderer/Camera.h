#pragma once

// -----------------------------------------------------------------------------
// Camera.h
//
// A first-person style fly camera for the editor viewport.
//
// The camera produces two matrices every frame:
//   - View matrix       : transforms world space into camera space
//   - Projection matrix : adds perspective (far = small, near = big)
//
// These matrices are passed to the vertex shader so every object in the
// scene is drawn from the camera's point of view.
// -----------------------------------------------------------------------------

#include <glm/glm.hpp>

struct GLFWwindow;

namespace Nova {

class Camera {
public:
    Camera() = default;
    Camera(float fovDegrees, float aspectRatio, float nearClip, float farClip);

    // Call every frame — handles keyboard + mouse input
    void OnUpdate(GLFWwindow* window, float deltaTime);

    // Call when the viewport panel is resized
    void SetAspectRatio(float aspectRatio);

    // Returns the view matrix (where the camera is / where it looks)
    const glm::mat4& GetViewMatrix()       const { return m_View; }

    // Returns the projection matrix (perspective)
    const glm::mat4& GetProjectionMatrix() const { return m_Projection; }

    // Camera world position (useful for lighting later)
    const glm::vec3& GetPosition()         const { return m_Position; }

private:
    void RecalculateView();
    void RecalculateProjection();

    // ── Camera state ──────────────────────────────────────────────────────────
    glm::vec3 m_Position    = { 0.0f, 0.0f,  3.0f }; // start 3 units back
    glm::vec3 m_Front       = { 0.0f, 0.0f, -1.0f }; // direction camera faces
    glm::vec3 m_Up          = { 0.0f, 1.0f,  0.0f }; // world up
    glm::vec3 m_Right       = { 1.0f, 0.0f,  0.0f }; // camera right

    // Euler angles (degrees)
    float m_Yaw   = -90.0f; // -90 so we start looking at -Z (into the scene)
    float m_Pitch =   0.0f;

    // ── Settings ──────────────────────────────────────────────────────────────
    float m_MoveSpeed    = 3.0f;  // units per second
    float m_MouseSensitivity = 0.1f;
    float m_Fov          = 60.0f;
    float m_AspectRatio  = 16.0f / 9.0f;
    float m_NearClip     = 0.1f;
    float m_FarClip      = 1000.0f;

    // ── Mouse state ───────────────────────────────────────────────────────────
    // We track the last mouse position to compute how much it moved each frame
    float m_LastMouseX   = 0.0f;
    float m_LastMouseY   = 0.0f;
    bool  m_FirstMouse   = true; // true until we get the first mouse position

    // ── Cached matrices ───────────────────────────────────────────────────────
    glm::mat4 m_View       = glm::mat4(1.0f);
    glm::mat4 m_Projection = glm::mat4(1.0f);
};

} // namespace Nova