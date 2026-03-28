#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdio>

namespace Nova {

// ── GLFW callbacks ────────────────────────────────────────────────────────────

static void GLFWErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

static void FramebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

// ── Application ───────────────────────────────────────────────────────────────

Application::Application(int width, int height, const char* title)
    : m_Width(width), m_Height(height), m_Title(title)
{
    Init();
}

Application::~Application() {
    Shutdown();
}

void Application::Init() {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    // Request OpenGL 4.6 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title, nullptr, nullptr);
    if (!m_Window)
        throw std::runtime_error("Failed to create GLFW window");

    glfwMakeContextCurrent(m_Window);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to initialize GLAD");

    std::printf("[Nova] OpenGL %s\n", (const char*)glGetString(GL_VERSION));
    std::printf("[Nova] Renderer: %s\n", (const char*)glGetString(GL_RENDERER));
}

void Application::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

void Application::ProcessInput() {
    if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(m_Window, true);
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        ProcessInput();

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ── future: renderer.Draw() goes here ──

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

} // namespace Nova