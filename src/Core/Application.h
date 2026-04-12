#pragma once

#include "Renderer/Shader.h"

#include <glad/glad.h>

struct GLFWwindow;

namespace Nova {

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    void Init();
    void Shutdown();
    void ProcessInput();

    // ImGui
    void ImGuiInit();
    void ImGuiBeginFrame();
    void ImGuiEndFrame();
    void ImGuiShutdown();
    void DrawEditorUI();

    // Renderer
    void RendererInit();
    void RendererShutdown();
    void RenderScene();

    // Framebuffer
    void CreateFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void ResizeFramebufferIfNeeded(int width, int height);

    GLFWwindow* m_Window = nullptr;
    int m_Width, m_Height;
    const char* m_Title;

    // Triangle
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    Shader m_Shader;

    // Framebuffer
    GLuint m_FBO         = 0;
    GLuint m_FBOColorTex = 0;
    GLuint m_FBODepthRBO = 0;
    int    m_FBOWidth    = 0;
    int    m_FBOHeight   = 0;

    // Timing
    float m_Time = 0.0f;
};

} // namespace Nova