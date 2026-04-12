// -----------------------------------------------------------------------------
// Camera.cpp
// -----------------------------------------------------------------------------

#include "Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm> // std::clamp

namespace Nova {

Camera::Camera(float fovDegrees, float aspectRatio, float nearClip, float farClip)
    : m_Fov(fovDegrees)
    , m_AspectRatio(aspectRatio)
    , m_NearClip(nearClip)
    , m_FarClip(farClip)
{
    RecalculateView();
    RecalculateProjection();
}

// -----------------------------------------------------------------------------
// OnUpdate — call every frame with the GLFW window and time since last frame
// -----------------------------------------------------------------------------

void Camera::OnUpdate(GLFWwindow* window, float deltaTime) {
    bool rightMouseHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // ── Mouse look (only when right mouse button is held) ─────────────────────
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (rightMouseHeld) {
        // Hide and lock cursor while looking around
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (m_FirstMouse) {
            // First frame of right-click: snap last position to current
            // so we don't get a sudden jump
            m_LastMouseX = (float)mouseX;
            m_LastMouseY = (float)mouseY;
            m_FirstMouse = false;
        }

        // How much the mouse moved since last frame
        float deltaX = ((float)mouseX - m_LastMouseX) * m_MouseSensitivity;
        float deltaY = (m_LastMouseY - (float)mouseY)  * m_MouseSensitivity; // flipped: up = positive pitch

        m_Yaw   += deltaX;
        m_Pitch += deltaY;

        // Clamp pitch so we can't flip upside down
        m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

        // Recalculate front direction from yaw + pitch
        glm::vec3 front;
        front.x = std::cos(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        front.y = std::sin(glm::radians(m_Pitch));
        front.z = std::sin(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
        m_Front = glm::normalize(front);

        // Recalculate right and up from front
        m_Right = glm::normalize(glm::cross(m_Front, m_Up));
    } else {
        // Right mouse released — show cursor again and reset first-mouse flag
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_FirstMouse = true;
    }

    m_LastMouseX = (float)mouseX;
    m_LastMouseY = (float)mouseY;

    // ── Keyboard movement (only when right mouse is held, like Unity/Blender) ─
    if (rightMouseHeld) {
        float speed = m_MoveSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            m_Position += m_Front * speed;  // forward

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            m_Position -= m_Front * speed;  // backward

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            m_Position -= m_Right * speed;  // left

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            m_Position += m_Right * speed;  // right

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            m_Position += m_Up * speed;     // up

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            m_Position -= m_Up * speed;     // down
    }

    RecalculateView();
}

// -----------------------------------------------------------------------------
// SetAspectRatio — call when the viewport panel is resized
// -----------------------------------------------------------------------------

void Camera::SetAspectRatio(float aspectRatio) {
    if (m_AspectRatio == aspectRatio) return;
    m_AspectRatio = aspectRatio;
    RecalculateProjection();
}

// -----------------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------------

void Camera::RecalculateView() {
    // lookAt(eye position, target position, up direction)
    m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

void Camera::RecalculateProjection() {
    // perspective(vertical fov, aspect ratio, near clip, far clip)
    m_Projection = glm::perspective(
        glm::radians(m_Fov),
        m_AspectRatio,
        m_NearClip,
        m_FarClip
    );
}

} // namespace Nova