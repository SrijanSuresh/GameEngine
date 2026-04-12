#pragma once

// -----------------------------------------------------------------------------
// Application.h
//
// The Application owns the window, OpenGL context, editor UI, active Scene,
// and the editor Camera. The main loop lives here.
// -----------------------------------------------------------------------------

#include "Renderer/Shader.h"
#include "Renderer/Camera.h"
#include "ECS/Scene.h"

#include <entt/entt.hpp>
#include <glad/glad.h>

struct GLFWwindow;

namespace Nova {

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    // Core lifecycle
    void Init();
    void Shutdown();
    void ProcessInput();

    // ImGui
    void ImGuiInit();
    void ImGuiBeginFrame();
    void ImGuiEndFrame();
    void ImGuiShutdown();
    void DrawEditorUI();

    // Editor panels
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();

    // Renderer
    void RendererInit();
    void RendererShutdown();
    void RenderScene();

    // Framebuffer
    void CreateFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void ResizeFramebufferIfNeeded(int width, int height);

    // ── Data ──────────────────────────────────────────────────────────────────

    GLFWwindow* m_Window = nullptr;
    int         m_Width;
    int         m_Height;
    const char* m_Title;

    // Active scene — owns all entities
    Scene m_Scene;

    // Selected entity in the Hierarchy (entt::null = nothing selected)
    entt::entity m_SelectedEntity = entt::null;

    // Editor camera
    Camera m_Camera;

    // Shader for all mesh renderers (will become a material system later)
    Shader m_Shader;

    // Framebuffer — scene renders here, then displayed in Viewport panel
    GLuint m_FBO         = 0;
    GLuint m_FBOColorTex = 0;
    GLuint m_FBODepthRBO = 0;
    int    m_FBOWidth    = 0;
    int    m_FBOHeight   = 0;

    // Timing
    float m_Time      = 0.0f;
    float m_LastTime  = 0.0f;
    float m_DeltaTime = 0.0f; // seconds since last frame — used for camera speed
};

} // namespace Nova