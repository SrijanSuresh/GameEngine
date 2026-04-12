// -----------------------------------------------------------------------------
// Application.cpp
// -----------------------------------------------------------------------------

#include "Application.h"
#include "ECS/Components.h"
#include "Generative/ShaderGenerator.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <string>

namespace Nova {

// -----------------------------------------------------------------------------
// Default shader source (used for entities with no MaterialComponent)
// -----------------------------------------------------------------------------

static const char* s_DefaultVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
out vec2 v_UV;
out vec3 v_WorldPos;
out vec3 v_Color;
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
void main() {
    v_Color    = a_Color;
    v_WorldPos = vec3(u_Model * vec4(a_Position, 1.0));
    v_UV       = a_Position.xy + 0.5;
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
)";

static const char* s_DefaultFragSrc = R"(
#version 330 core
in vec3 v_Color;
in vec2 v_UV;
out vec4 FragColor;
uniform float u_Time;
void main() {
    float pulse = 0.5 + 0.5 * sin(u_Time * 2.0);
    FragColor   = vec4(v_Color * pulse, 1.0);
}
)";

// -----------------------------------------------------------------------------
// GLFW callbacks
// -----------------------------------------------------------------------------

static void GLFWErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

static void FramebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------

Application::Application(int width, int height, const char* title)
    : m_Width(width), m_Height(height), m_Title(title)
{
    Init();
}

Application::~Application() {
    Shutdown();
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

void Application::Init() {
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title, nullptr, nullptr);
    if (!m_Window)
        throw std::runtime_error("Failed to create GLFW window");

    glfwMakeContextCurrent(m_Window);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to initialize GLAD");

    std::printf("[Nova] OpenGL %s\n",    (const char*)glGetString(GL_VERSION));
    std::printf("[Nova] Renderer: %s\n", (const char*)glGetString(GL_RENDERER));

    ImGuiInit();
    RendererInit();

    m_Camera = Camera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
}

// -----------------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------------

void Application::Shutdown() {
    RendererShutdown();
    ImGuiShutdown();

    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

// -----------------------------------------------------------------------------
// ProcessInput
// -----------------------------------------------------------------------------

void Application::ProcessInput() {
    if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(m_Window, true);
}

// -----------------------------------------------------------------------------
// RendererInit
// -----------------------------------------------------------------------------

void Application::RendererInit() {
    m_DefaultShader = Shader(s_DefaultVertSrc, s_DefaultFragSrc);

    float vertices[] = {
        // position           // color
         0.0f,  0.5f,  0.0f,  1.0f, 0.2f, 0.3f,
        -0.5f, -0.5f,  0.0f,  0.2f, 0.8f, 0.4f,
         0.5f, -0.5f,  0.0f,  0.2f, 0.4f, 1.0f,
    };

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    entt::entity triangle = m_Scene.CreateEntity("Triangle");
    m_Scene.GetRegistry().emplace<MeshRendererComponent>(triangle, vao, vbo, 3);
    m_SelectedEntity = triangle;

    CreateFramebuffer(1280, 720);
}

// -----------------------------------------------------------------------------
// RendererShutdown
// -----------------------------------------------------------------------------

void Application::RendererShutdown() {
    DestroyFramebuffer();

    auto view = m_Scene.GetRegistry().view<MeshRendererComponent>();
    for (auto entity : view) {
        auto& mesh = view.get<MeshRendererComponent>(entity);
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1,      &mesh.VBO);
    }
}

// -----------------------------------------------------------------------------
// RenderScene — uses per-entity MaterialComponent shader if present,
//               otherwise falls back to the default shader
// -----------------------------------------------------------------------------

void Application::RenderScene() {
    auto view = m_Scene.GetRegistry().view<MeshRendererComponent, TransformComponent>();

    for (auto entity : view) {
        auto& mesh      = view.get<MeshRendererComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // Choose shader: generated material or default
        Shader* shader = &m_DefaultShader;
        auto& registry = m_Scene.GetRegistry();
        if (registry.all_of<MaterialComponent>(entity)) {
            auto& mat = registry.get<MaterialComponent>(entity);
            if (mat.IsGenerated && mat.Mat && mat.Mat->IsValid())
                shader = mat.Mat.get();
        }

        // Build model matrix from TransformComponent
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.Position);
        model = glm::rotate(model, glm::radians(transform.Rotation.x), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(transform.Rotation.y), glm::vec3(0,1,0));
        model = glm::rotate(model, glm::radians(transform.Rotation.z), glm::vec3(0,0,1));
        model = glm::scale(model, transform.Scale);

        shader->Bind();
        shader->SetFloat("u_Time",       m_Time);
        shader->SetMat4 ("u_Model",      model);
        shader->SetMat4 ("u_View",       m_Camera.GetViewMatrix());
        shader->SetMat4 ("u_Projection", m_Camera.GetProjectionMatrix());

        glBindVertexArray(mesh.VAO);
        glDrawArrays(GL_TRIANGLES, 0, mesh.VertexCount);
        glBindVertexArray(0);

        shader->Unbind();
    }
}

// -----------------------------------------------------------------------------
// GenerateShaderForEntity
// Builds GLSL from prompt, compiles it, attaches to entity's MaterialComponent
// -----------------------------------------------------------------------------

void Application::GenerateShaderForEntity(entt::entity entity, const std::string& prompt) {
    m_LastShaderError = "";

    // Generate GLSL source strings from the prompt
    std::string vertSrc = ShaderGenerator::GetVertexSource();
    std::string fragSrc = ShaderGenerator::Generate(prompt);

    // Compile into a new Shader
    auto newShader = std::make_shared<Shader>(vertSrc.c_str(), fragSrc.c_str());

    if (!newShader->IsValid()) {
        m_LastShaderError = "Shader compilation failed. Check console for GLSL errors.";
        std::fprintf(stderr, "[Nova] Generated shader failed to compile for prompt: '%s'\n",
                     prompt.c_str());
        return;
    }

    // Attach or update MaterialComponent on this entity
    auto& registry = m_Scene.GetRegistry();
    if (!registry.all_of<MaterialComponent>(entity))
        registry.emplace<MaterialComponent>(entity);

    auto& mat       = registry.get<MaterialComponent>(entity);
    mat.Prompt      = prompt;
    mat.Mat         = newShader;
    mat.IsGenerated = true;

    std::printf("[Nova] Generated shader for prompt: '%s'\n", prompt.c_str());
}

// -----------------------------------------------------------------------------
// Framebuffer
// -----------------------------------------------------------------------------

void Application::CreateFramebuffer(int width, int height) {
    m_FBOWidth  = width;
    m_FBOHeight = height;

    glGenTextures(1, &m_FBOColorTex);
    glBindTexture(GL_TEXTURE_2D, m_FBOColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &m_FBODepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_FBODepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_FBOColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_FBODepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[Nova] Framebuffer incomplete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Application::DestroyFramebuffer() {
    if (m_FBO)         { glDeleteFramebuffers(1,  &m_FBO);         m_FBO         = 0; }
    if (m_FBOColorTex) { glDeleteTextures(1,      &m_FBOColorTex); m_FBOColorTex = 0; }
    if (m_FBODepthRBO) { glDeleteRenderbuffers(1, &m_FBODepthRBO); m_FBODepthRBO = 0; }
}

void Application::ResizeFramebufferIfNeeded(int width, int height) {
    if (width == m_FBOWidth && height == m_FBOHeight) return;
    if (width <= 0 || height <= 0) return;
    DestroyFramebuffer();
    CreateFramebuffer(width, height);
}

// -----------------------------------------------------------------------------
// ImGui
// -----------------------------------------------------------------------------

void Application::ImGuiInit() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGuiStyle& style       = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 4);
    style.ScrollbarRounding = 3.0f;
    style.TabRounding       = 3.0f;

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

void Application::ImGuiShutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::ImGuiBeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::ImGuiEndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// -----------------------------------------------------------------------------
// Editor panels
// -----------------------------------------------------------------------------

void Application::DrawEditorUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking             |
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus            |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::Begin("##DockSpace", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit"))
                glfwSetWindowShouldClose(m_Window, true);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    DrawHierarchyPanel();
    DrawInspectorPanel();
    DrawViewportPanel();
    DrawConsolePanel();
}

void Application::DrawHierarchyPanel() {
    ImGui::Begin("Hierarchy");

    if (ImGui::Button("+ Add Entity"))
        m_Scene.CreateEntity("New Entity");

    ImGui::Separator();

    auto view = m_Scene.GetRegistry().view<TagComponent>();
    for (auto entity : view) {
        auto& tag        = view.get<TagComponent>(entity);
        bool  isSelected = (entity == m_SelectedEntity);
        if (ImGui::Selectable(tag.Name.c_str(), isSelected))
            m_SelectedEntity = entity;
    }

    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
        m_SelectedEntity = entt::null;

    ImGui::End();
}

void Application::DrawInspectorPanel() {
    ImGui::Begin("Inspector");

    if (m_SelectedEntity == entt::null || !m_Scene.IsValid(m_SelectedEntity)) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    auto& registry = m_Scene.GetRegistry();

    // ── Tag ───────────────────────────────────────────────────────────────────
    if (registry.all_of<TagComponent>(m_SelectedEntity)) {
        auto& tag = registry.get<TagComponent>(m_SelectedEntity);
        char  nameBuf[256];
        std::strncpy(nameBuf, tag.Name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            tag.Name = nameBuf;
    }

    ImGui::Separator();

    // ── Transform ─────────────────────────────────────────────────────────────
    if (registry.all_of<TransformComponent>(m_SelectedEntity)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& t = registry.get<TransformComponent>(m_SelectedEntity);
            ImGui::DragFloat3("Position", &t.Position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &t.Rotation.x, 1.0f);
            ImGui::DragFloat3("Scale",    &t.Scale.x,    0.1f);
        }
    }

    ImGui::Separator();

    // ── Mesh Renderer ─────────────────────────────────────────────────────────
    if (registry.all_of<MeshRendererComponent>(m_SelectedEntity)) {
        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mesh = registry.get<MeshRendererComponent>(m_SelectedEntity);
            ImGui::Text("VAO ID:       %u", mesh.VAO);
            ImGui::Text("Vertex Count: %d", mesh.VertexCount);
        }
    }

    ImGui::Separator();

    // ── Material (Generative Shader) ──────────────────────────────────────────
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Show current prompt if one exists
        std::string currentPrompt = "";
        if (registry.all_of<MaterialComponent>(m_SelectedEntity))
            currentPrompt = registry.get<MaterialComponent>(m_SelectedEntity).Prompt;

        // Static buffer for the text input — persists across frames
        static char promptBuf[256] = "";

        // When a new entity is selected, sync the buffer to its current prompt
        static entt::entity lastInspected = entt::null;
        if (lastInspected != m_SelectedEntity) {
            std::strncpy(promptBuf, currentPrompt.c_str(), sizeof(promptBuf));
            lastInspected   = m_SelectedEntity;
            m_LastShaderError = "";
        }

        // Hint text listing available keywords
        ImGui::TextDisabled("Keywords: galaxy fire ocean nebula cyberpunk");
        ImGui::TextDisabled("          lava aurora glitch plasma void");
        ImGui::TextDisabled("Combine: \"fire galaxy\" blends both effects");
        ImGui::Spacing();

        // Text input — pressing Enter triggers generation
        ImGui::SetNextItemWidth(-1.0f); // full width
        bool pressedEnter = ImGui::InputText(
            "##prompt", promptBuf, sizeof(promptBuf),
            ImGuiInputTextFlags_EnterReturnsTrue
        );

        // Generate button OR Enter key both trigger generation
        bool clickedGenerate = ImGui::Button("Generate  (or press Enter)");

        if (pressedEnter || clickedGenerate) {
            GenerateShaderForEntity(m_SelectedEntity, std::string(promptBuf));
        }

        // Show success or error state
        if (!m_LastShaderError.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", m_LastShaderError.c_str());
            ImGui::PopStyleColor();
        } else if (registry.all_of<MaterialComponent>(m_SelectedEntity)) {
            auto& mat = registry.get<MaterialComponent>(m_SelectedEntity);
            if (mat.IsGenerated) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.5f, 1.0f));
                ImGui::Text("✓ Shader active: \"%s\"", mat.Prompt.c_str());
                ImGui::PopStyleColor();
            }
        }

        // Clear button — removes generated material, reverts to default shader
        if (registry.all_of<MaterialComponent>(m_SelectedEntity)) {
            ImGui::Spacing();
            if (ImGui::Button("Clear Material")) {
                registry.remove<MaterialComponent>(m_SelectedEntity);
                std::memset(promptBuf, 0, sizeof(promptBuf));
                m_LastShaderError = "";
            }
        }
    }

    ImGui::End();
}

void Application::DrawViewportPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ResizeFramebufferIfNeeded((int)viewportSize.x, (int)viewportSize.y);

    if (viewportSize.y > 0)
        m_Camera.SetAspectRatio(viewportSize.x / viewportSize.y);

    if (ImGui::IsWindowHovered())
        ImGui::SetTooltip("Right-click + drag to look\nWASD / Q / E to move");

    ImGui::Image(
        (ImTextureID)(intptr_t)m_FBOColorTex,
        viewportSize,
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    ImGui::End();
    ImGui::PopStyleVar();
}

void Application::DrawConsolePanel() {
    ImGui::Begin("Console");
    ImGui::Text("[Nova] Time: %.2f  |  FPS: %.0f", m_Time, 1.0f / m_DeltaTime);
    ImGui::Text("[Nova] Camera: (%.2f, %.2f, %.2f)",
        m_Camera.GetPosition().x,
        m_Camera.GetPosition().y,
        m_Camera.GetPosition().z);
    ImGui::Text("[Nova] Entities: %d",
        (int)m_Scene.GetRegistry().storage<entt::entity>().in_use());
    ImGui::End();
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        m_Time      = (float)glfwGetTime();
        m_DeltaTime = m_Time - m_LastTime;
        m_LastTime  = m_Time;

        ProcessInput();
        m_Camera.OnUpdate(m_Window, m_DeltaTime);

        // Render scene into FBO
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, m_FBOWidth, m_FBOHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderScene();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear main window
        int w, h;
        glfwGetFramebufferSize(m_Window, &w, &h);
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw editor UI
        ImGuiBeginFrame();
        DrawEditorUI();
        ImGuiEndFrame();

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

} // namespace Nova