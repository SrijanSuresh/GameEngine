#pragma once

// -----------------------------------------------------------------------------
// Application.h
// -----------------------------------------------------------------------------

#include "Renderer/Shader.h"
#include "Renderer/Camera.h"
#include "ECS/Scene.h"

#include <entt/entt.hpp>
#include <glad/glad.h>

#include <string>

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

    // Editor panels
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawPromptPanel();

    // Renderer
    void RendererInit();
    void RendererShutdown();
    void RenderScene();
    entt::entity CreateTriangleEntity(const std::string& name);
    void DestroyEntity(entt::entity entity);
    bool ExecutePromptCommand(const std::string& command);

    // Generative shader — compiles a new shader from prompt for selected entity
    void GenerateShaderForEntity(entt::entity entity, const std::string& prompt);

    // Framebuffer
    void CreateFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void ResizeFramebufferIfNeeded(int width, int height);

    // ── Data ──────────────────────────────────────────────────────────────────

    GLFWwindow* m_Window = nullptr;
    int         m_Width;
    int         m_Height;
    const char* m_Title;

    Scene        m_Scene;
    entt::entity m_SelectedEntity = entt::null;
    Camera       m_Camera;

    // Default shader — used for entities without a MaterialComponent
    Shader m_DefaultShader;

    // Framebuffer
    GLuint m_FBO         = 0;
    GLuint m_FBOColorTex = 0;
    GLuint m_FBODepthRBO = 0;
    int    m_FBOWidth    = 0;
    int    m_FBOHeight   = 0;

    // Timing
    float m_Time      = 0.0f;
    float m_LastTime  = 0.0f;
    float m_DeltaTime = 0.0f;

    // Stores the last shader compilation error for display in the Inspector
    std::string m_LastShaderError = "";
    std::string m_CommandStatus   = "";
    bool        m_CommandHadError = false;
    int         m_EntityCounter   = 1;
};

} // namespace Nova
