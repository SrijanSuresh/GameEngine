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
#include <cmath>
#include <sstream>
#include <string>
#include <algorithm>

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

static std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

static bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static bool TryParseVec3(const std::string& text, glm::vec3& outVec) {
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');

    std::istringstream stream(normalized);
    stream >> outVec.x >> outVec.y >> outVec.z;
    return !stream.fail();
}

static entt::entity FindEntityByName(Scene& scene, const std::string& name) {
    const std::string needle = ToLower(Trim(name));
    if (needle.empty())
        return entt::null;

    auto view = scene.GetRegistry().view<TagComponent>();
    for (auto entity : view) {
        const auto& tag = view.get<TagComponent>(entity);
        if (ToLower(tag.Name) == needle)
            return entity;
    }

    return entt::null;
}

static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
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
    m_SelectedEntity = CreateTriangleEntity("Triangle");

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

entt::entity Application::CreateTriangleEntity(const std::string& name) {
    static constexpr float kTriangleVertices[] = {
        // position           // color
         0.0f,  0.5f,  0.0f,  1.0f, 0.2f, 0.3f,
        -0.5f, -0.5f,  0.0f,  0.2f, 0.8f, 0.4f,
         0.5f, -0.5f,  0.0f,  0.2f, 0.4f, 1.0f,
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kTriangleVertices), kTriangleVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    entt::entity entity = m_Scene.CreateEntity(name);
    m_Scene.GetRegistry().emplace<MeshRendererComponent>(entity, vao, vbo, 3);
    return entity;
}

void Application::DestroyEntity(entt::entity entity) {
    if (!m_Scene.IsValid(entity))
        return;

    auto& registry = m_Scene.GetRegistry();
    if (registry.all_of<MeshRendererComponent>(entity)) {
        auto& mesh = registry.get<MeshRendererComponent>(entity);
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
    }

    m_Scene.DestroyEntity(entity);
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

bool Application::ExecutePromptCommand(const std::string& command) {
    const std::string trimmed = Trim(command);
    const std::string lower   = ToLower(trimmed);

    if (trimmed.empty()) {
        m_CommandStatus = "Type a command first.";
        m_CommandHadError = true;
        return false;
    }

    if ((StartsWith(lower, "create ") || StartsWith(lower, "spawn ") || StartsWith(lower, "add ")) &&
        lower.find("triangle") != std::string::npos) {
        std::string name;
        const size_t namedPos = lower.find(" named ");
        if (namedPos != std::string::npos)
            name = Trim(trimmed.substr(namedPos + 7));

        if (name.empty())
            name = "Triangle " + std::to_string(++m_EntityCounter);

        m_SelectedEntity = CreateTriangleEntity(name);
        m_CommandStatus = "Created triangle \"" + name + "\".";
        m_CommandHadError = false;
        return true;
    }

    if (lower == "delete selected") {
        if (m_SelectedEntity == entt::null || !m_Scene.IsValid(m_SelectedEntity)) {
            m_CommandStatus = "No selected entity to delete.";
            m_CommandHadError = true;
            return false;
        }

        std::string deletedName = "entity";
        if (m_Scene.GetRegistry().all_of<TagComponent>(m_SelectedEntity))
            deletedName = m_Scene.GetRegistry().get<TagComponent>(m_SelectedEntity).Name;

        DestroyEntity(m_SelectedEntity);
        m_SelectedEntity = entt::null;
        m_CommandStatus = "Deleted \"" + deletedName + "\".";
        m_CommandHadError = false;
        return true;
    }

    if (StartsWith(lower, "delete ")) {
        const std::string name = Trim(trimmed.substr(7));
        entt::entity entity = FindEntityByName(m_Scene, name);
        if (entity == entt::null) {
            m_CommandStatus = "Couldn't find an entity named \"" + name + "\".";
            m_CommandHadError = true;
            return false;
        }

        DestroyEntity(entity);
        if (entity == m_SelectedEntity)
            m_SelectedEntity = entt::null;

        m_CommandStatus = "Deleted \"" + name + "\".";
        m_CommandHadError = false;
        return true;
    }

    if (StartsWith(lower, "select ")) {
        const std::string name = Trim(trimmed.substr(7));
        entt::entity entity = FindEntityByName(m_Scene, name);
        if (entity == entt::null) {
            m_CommandStatus = "Couldn't find an entity named \"" + name + "\".";
            m_CommandHadError = true;
            return false;
        }

        m_SelectedEntity = entity;
        m_CommandStatus = "Selected \"" + name + "\".";
        m_CommandHadError = false;
        return true;
    }

    if (StartsWith(lower, "rename selected to ")) {
        if (m_SelectedEntity == entt::null || !m_Scene.IsValid(m_SelectedEntity)) {
            m_CommandStatus = "Select an entity before renaming it.";
            m_CommandHadError = true;
            return false;
        }

        const std::string name = Trim(trimmed.substr(19));
        if (name.empty()) {
            m_CommandStatus = "Give the selected entity a new name.";
            m_CommandHadError = true;
            return false;
        }

        m_Scene.GetRegistry().get<TagComponent>(m_SelectedEntity).Name = name;
        m_CommandStatus = "Renamed the selected entity to \"" + name + "\".";
        m_CommandHadError = false;
        return true;
    }

    if (StartsWith(lower, "move selected to ")) {
        if (m_SelectedEntity == entt::null || !m_Scene.IsValid(m_SelectedEntity)) {
            m_CommandStatus = "Select an entity before moving it.";
            m_CommandHadError = true;
            return false;
        }

        glm::vec3 position;
        if (!TryParseVec3(trimmed.substr(17), position)) {
            m_CommandStatus = "Use three numbers for move commands, like: move selected to 1 2 0";
            m_CommandHadError = true;
            return false;
        }

        m_Scene.GetRegistry().get<TransformComponent>(m_SelectedEntity).Position = position;
        m_CommandStatus = "Moved the selected entity.";
        m_CommandHadError = false;
        return true;
    }

    if (StartsWith(lower, "move ")) {
        const size_t toPos = lower.find(" to ");
        if (toPos != std::string::npos) {
            const std::string name = Trim(trimmed.substr(5, toPos - 5));
            glm::vec3 position;
            if (!TryParseVec3(trimmed.substr(toPos + 4), position)) {
                m_CommandStatus = "Use three numbers for move commands, like: move Player to 1 2 0";
                m_CommandHadError = true;
                return false;
            }

            entt::entity entity = FindEntityByName(m_Scene, name);
            if (entity == entt::null) {
                m_CommandStatus = "Couldn't find an entity named \"" + name + "\".";
                m_CommandHadError = true;
                return false;
            }

            m_Scene.GetRegistry().get<TransformComponent>(entity).Position = position;
            m_SelectedEntity = entity;
            m_CommandStatus = "Moved \"" + name + "\".";
            m_CommandHadError = false;
            return true;
        }
    }

    if (StartsWith(lower, "material ")) {
        if (m_SelectedEntity == entt::null || !m_Scene.IsValid(m_SelectedEntity)) {
            m_CommandStatus = "Select an entity before applying a material prompt.";
            m_CommandHadError = true;
            return false;
        }

        const std::string prompt = Trim(trimmed.substr(9));
        if (prompt.empty()) {
            m_CommandStatus = "Type a material description after the word material.";
            m_CommandHadError = true;
            return false;
        }

        GenerateShaderForEntity(m_SelectedEntity, prompt);
        m_CommandHadError = !m_LastShaderError.empty();
        m_CommandStatus = m_CommandHadError
            ? m_LastShaderError
            : "Applied material prompt \"" + prompt + "\".";
        return !m_CommandHadError;
    }

    m_CommandStatus =
        "Unknown command. Try create triangle named Player, select Player, move selected to 1 0 0, or material fire galaxy.";
    m_CommandHadError = true;
    return false;
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

    ImGuiStyle& style       = ImGui::GetStyle();
    style.WindowRounding    = 16.0f;
    style.ChildRounding     = 14.0f;
    style.FrameRounding     = 10.0f;
    style.PopupRounding     = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 10.0f;
    style.TabRounding       = 10.0f;
    style.WindowPadding     = ImVec2(14.0f, 12.0f);
    style.FramePadding      = ImVec2(10.0f, 8.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;
    style.IndentSpacing     = 18.0f;

    ApplyTheme();

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

void Application::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    const float t = m_ThemeBlend;

    const ImVec4 inkWindow   = ImVec4(0.08f, 0.10f, 0.14f, 0.96f);
    const ImVec4 mistWindow  = ImVec4(0.95f, 0.96f, 0.98f, 0.96f);
    const ImVec4 inkPanel    = ImVec4(0.11f, 0.14f, 0.19f, 0.96f);
    const ImVec4 mistPanel   = ImVec4(0.98f, 0.985f, 0.995f, 0.97f);
    const ImVec4 inkSurface  = ImVec4(0.15f, 0.18f, 0.24f, 1.0f);
    const ImVec4 mistSurface = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    const ImVec4 inkAccent   = ImVec4(0.37f, 0.67f, 0.95f, 1.0f);
    const ImVec4 mistAccent  = ImVec4(0.18f, 0.48f, 0.91f, 1.0f);
    const ImVec4 inkHover    = ImVec4(0.45f, 0.74f, 0.98f, 1.0f);
    const ImVec4 mistHover   = ImVec4(0.29f, 0.58f, 0.95f, 1.0f);
    const ImVec4 inkText     = ImVec4(0.92f, 0.95f, 0.99f, 1.0f);
    const ImVec4 mistText    = ImVec4(0.16f, 0.20f, 0.28f, 1.0f);
    const ImVec4 inkMuted    = ImVec4(0.59f, 0.67f, 0.79f, 1.0f);
    const ImVec4 mistMuted   = ImVec4(0.45f, 0.51f, 0.61f, 1.0f);
    const ImVec4 inkBorder   = ImVec4(0.27f, 0.35f, 0.47f, 0.36f);
    const ImVec4 mistBorder  = ImVec4(0.54f, 0.63f, 0.76f, 0.22f);

    colors[ImGuiCol_Text]                 = LerpColor(inkText, mistText, t);
    colors[ImGuiCol_TextDisabled]         = LerpColor(inkMuted, mistMuted, t);
    colors[ImGuiCol_WindowBg]             = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_ChildBg]              = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_PopupBg]              = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_Border]               = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg]              = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_FrameBgHovered]       = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_FrameBgActive]        = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_TitleBg]              = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_TitleBgActive]        = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_MenuBarBg]            = LerpColor(
        ImVec4(0.06f, 0.08f, 0.12f, 0.98f),
        ImVec4(0.93f, 0.95f, 0.98f, 0.98f),
        t
    );
    colors[ImGuiCol_ScrollbarBg]          = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_ScrollbarGrab]        = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_ScrollbarGrabHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_ScrollbarGrabActive]  = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_CheckMark]            = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_SliderGrab]           = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_SliderGrabActive]     = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_Button]               = LerpColor(
        ImVec4(0.18f, 0.24f, 0.32f, 0.90f),
        ImVec4(0.86f, 0.90f, 0.96f, 0.95f),
        t
    );
    colors[ImGuiCol_ButtonHovered]        = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_ButtonActive]         = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_Header]               = LerpColor(
        ImVec4(0.19f, 0.26f, 0.36f, 0.90f),
        ImVec4(0.83f, 0.89f, 0.97f, 0.98f),
        t
    );
    colors[ImGuiCol_HeaderHovered]        = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_HeaderActive]         = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_Separator]            = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_SeparatorHovered]     = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_SeparatorActive]      = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_ResizeGrip]           = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_ResizeGripHovered]    = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_ResizeGripActive]     = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_Tab]                  = LerpColor(
        ImVec4(0.13f, 0.17f, 0.23f, 0.95f),
        ImVec4(0.89f, 0.92f, 0.97f, 0.95f),
        t
    );
    colors[ImGuiCol_TabHovered]           = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_TabActive]            = LerpColor(
        ImVec4(0.17f, 0.23f, 0.32f, 0.98f),
        ImVec4(0.84f, 0.89f, 0.97f, 0.98f),
        t
    );
    colors[ImGuiCol_TabUnfocused]         = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_TabUnfocusedActive]   = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_DockingEmptyBg]       = LerpColor(
        ImVec4(0.05f, 0.07f, 0.10f, 1.0f),
        ImVec4(0.91f, 0.94f, 0.98f, 1.0f),
        t
    );
    colors[ImGuiCol_PlotLines]            = LerpColor(inkMuted, mistMuted, t);
    colors[ImGuiCol_PlotLinesHovered]     = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_PlotHistogram]        = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_PlotHistogramHovered] = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_TextSelectedBg]       = LerpColor(
        ImVec4(0.30f, 0.56f, 0.90f, 0.35f),
        ImVec4(0.27f, 0.53f, 0.92f, 0.30f),
        t
    );
}

void Application::UpdateThemeAnimation() {
    const float targetBlend = (m_Theme == EditorTheme::Mist) ? 1.0f : 0.0f;
    const float blendSpeed = std::min(m_DeltaTime * 8.0f, 1.0f);
    m_ThemeBlend += (targetBlend - m_ThemeBlend) * blendSpeed;

    if (std::abs(targetBlend - m_ThemeBlend) < 0.001f)
        m_ThemeBlend = targetBlend;

    ApplyTheme();
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
    UpdateThemeAnimation();
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

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 172.0f);
        DrawThemeToggle();
        ImGui::EndMenuBar();
    }

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceID, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    DrawHierarchyPanel();
    DrawInspectorPanel();
    DrawViewportPanel();
    DrawPromptPanel();
    DrawConsolePanel();
}

void Application::DrawThemeToggle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));

    const bool inkActive  = (m_Theme == EditorTheme::Ink);
    const bool mistActive = (m_Theme == EditorTheme::Mist);

    if (inkActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("Ink")) {
        m_Theme = EditorTheme::Ink;
    }
    if (inkActive) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 8.0f);

    if (mistActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("Mist")) {
        m_Theme = EditorTheme::Mist;
    }
    if (mistActive) {
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar(2);
}

void Application::DrawHierarchyPanel() {
    ImGui::Begin("Hierarchy");

    if (ImGui::Button("+ Add Entity")) {
        const std::string name = "Triangle " + std::to_string(++m_EntityCounter);
        m_SelectedEntity = CreateTriangleEntity(name);
        m_CommandStatus = "Created triangle \"" + name + "\" from the hierarchy.";
        m_CommandHadError = false;
    }

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
        char  nameBuf[256] = {};
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", tag.Name.c_str());
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
            std::snprintf(promptBuf, sizeof(promptBuf), "%s", currentPrompt.c_str());
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

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max = ImVec2(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    const ImU32 frameColor = ImGui::GetColorU32(ImVec4(0.40f, 0.62f, 0.96f, 0.18f + 0.10f * (1.0f - m_ThemeBlend)));
    drawList->AddRect(min, max, frameColor, 16.0f, 0, 2.0f);

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
    const float fps = (m_DeltaTime > 0.0f) ? (1.0f / m_DeltaTime) : 0.0f;
    ImGui::Text("[Nova] Time: %.2f  |  FPS: %.0f", m_Time, fps);
    ImGui::Text("[Nova] Camera: (%.2f, %.2f, %.2f)",
        m_Camera.GetPosition().x,
        m_Camera.GetPosition().y,
        m_Camera.GetPosition().z);
    ImGui::Text("[Nova] Entities: %d",
        (int)m_Scene.GetRegistry().storage<entt::entity>().size());
    if (!m_CommandStatus.empty())
        ImGui::TextWrapped("[Nova] Prompt: %s", m_CommandStatus.c_str());
    ImGui::End();
}

void Application::DrawPromptPanel() {
    ImGui::Begin("Prompt");

    ImGui::TextWrapped("Drive scene editing with simple commands. Nova should feel closer to a creative desktop app than a heavy editor.");
    ImGui::TextDisabled("Examples:");
    ImGui::BulletText("create triangle named Player");
    ImGui::BulletText("select Player");
    ImGui::BulletText("move selected to 1 0 0");
    ImGui::BulletText("material aurora glitch");
    ImGui::BulletText("delete selected");
    ImGui::Spacing();

    static char commandBuf[256] = "";
    ImGui::SetNextItemWidth(-1.0f);
    const bool pressedEnter = ImGui::InputText(
        "##scene-command",
        commandBuf,
        sizeof(commandBuf),
        ImGuiInputTextFlags_EnterReturnsTrue
    );

    const bool clickedRun = ImGui::Button("Run Command");
    if (pressedEnter || clickedRun) {
        ExecutePromptCommand(commandBuf);
    }

    if (!m_CommandStatus.empty()) {
        ImGui::Spacing();
        const ImVec4 color = m_CommandHadError
            ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
            : ImVec4(0.35f, 1.0f, 0.55f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", m_CommandStatus.c_str());
        ImGui::PopStyleColor();
    }

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
        const ImVec4 sceneClear = LerpColor(
            ImVec4(0.12f, 0.13f, 0.17f, 1.0f),
            ImVec4(0.83f, 0.88f, 0.95f, 1.0f),
            m_ThemeBlend
        );
        glClearColor(sceneClear.x, sceneClear.y, sceneClear.z, sceneClear.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderScene();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear main window
        int w, h;
        glfwGetFramebufferSize(m_Window, &w, &h);
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        const ImVec4 appClear = LerpColor(
            ImVec4(0.06f, 0.08f, 0.11f, 1.0f),
            ImVec4(0.92f, 0.95f, 0.985f, 1.0f),
            m_ThemeBlend
        );
        glClearColor(appClear.x, appClear.y, appClear.z, appClear.w);
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
