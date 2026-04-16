#pragma once

#include "Commands/SceneCommand.h"
#include "Commands/SceneCommandHistory.h"
#include "ECS/Scene.h"
#include "Renderer/Camera.h"
#include "Renderer/Shader.h"

#include <entt/entt.hpp>
#include <glad/glad.h>

#include <string>
#include <vector>

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
    void ApplyTheme();
    void UpdateThemeAnimation();
    void DrawEditorUI();

    // Editor panels
    void DrawHierarchyPanel();
    void DrawInspectorPanel();
    void DrawViewportPanel();
    void DrawConsolePanel();
    void DrawPromptPanel();
    void DrawThemeToggle();

    // Renderer
    void RendererInit();
    void RendererShutdown();
    void RenderScene();
    entt::entity CreatePrimitiveEntity(PrimitiveType primitive, const std::string& name);
    void DestroyRuntimeEntity(entt::entity entity);
    std::string MakeUniqueEntityName(const std::string& baseName) const;

    // Commands and persistence
    EntitySnapshot CaptureEntitySnapshot(entt::entity entity) const;
    std::vector<EntitySnapshot> CaptureSceneSnapshots() const;
    entt::entity RestoreEntityFromSnapshot(const EntitySnapshot& snapshot);
    void ClearScene();
    CommandExecutionResult ExecuteSceneCommand(const SceneCommand& command, bool trackHistory = true);
    bool ExecutePromptInput(const std::string& input);
    void UndoLastCommand();
    void RedoLastCommand();
    bool SaveScene(const std::string& path);
    bool LoadScene(const std::string& path);

    // Materials
    bool GenerateShaderForEntity(entt::entity entity, const std::string& prompt);
    void ClearMaterialForEntity(entt::entity entity);

    // Framebuffer
    void CreateFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void ResizeFramebufferIfNeeded(int width, int height);

    enum class EditorTheme {
        Ink,
        Mist
    };

    GLFWwindow* m_Window = nullptr;
    int         m_Width;
    int         m_Height;
    const char* m_Title;

    Scene              m_Scene;
    SceneCommandHistory m_History;
    entt::entity       m_SelectedEntity = entt::null;
    Camera             m_Camera;
    Shader             m_DefaultShader;

    GLuint m_FBO         = 0;
    GLuint m_FBOColorTex = 0;
    GLuint m_FBODepthRBO = 0;
    int    m_FBOWidth    = 0;
    int    m_FBOHeight   = 0;

    float m_Time      = 0.0f;
    float m_LastTime  = 0.0f;
    float m_DeltaTime = 0.0f;

    std::string m_LastShaderError = "";
    std::string m_CommandStatus   = "";
    bool        m_CommandHadError = false;
    int         m_EntityCounter   = 1;
    EditorTheme m_Theme           = EditorTheme::Ink;
    float       m_ThemeBlend      = 0.0f;
    std::string m_DefaultScenePath = "assets/scenes/default_scene.json";
};

} // namespace Nova
