#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdexcept>
#include <cstdio>

namespace Nova {

// ── Shader sources (inline for now, will move to files later) ─────────────────

static const char* s_VertSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
out vec3 v_Color;
void main() {
    v_Color     = a_Color;
    gl_Position = vec4(a_Position, 1.0);
}
)";

static const char* s_FragSrc = R"(
#version 330 core
in  vec3 v_Color;
out vec4 FragColor;
uniform float u_Time;
void main() {
    float pulse = 0.5 + 0.5 * sin(u_Time * 2.0);
    FragColor   = vec4(v_Color * pulse, 1.0);
}
)";

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

    std::printf("[Nova] OpenGL %s\n", (const char*)glGetString(GL_VERSION));
    std::printf("[Nova] Renderer: %s\n", (const char*)glGetString(GL_RENDERER));

    ImGuiInit();
    RendererInit();
}

void Application::Shutdown() {
    RendererShutdown();
    ImGuiShutdown();

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

// ── Renderer ──────────────────────────────────────────────────────────────────

void Application::RendererInit() {
    // Compile shader
    m_Shader = Shader(s_VertSrc, s_FragSrc);

    // Triangle: position (xyz) + color (rgb) interleaved
    float vertices[] = {
        // position          // color
         0.0f,  0.5f, 0.0f,  1.0f, 0.2f, 0.3f,  // top    — red
        -0.5f, -0.5f, 0.0f,  0.2f, 0.8f, 0.4f,  // left   — green
         0.5f, -0.5f, 0.0f,  0.2f, 0.4f, 1.0f,  // right  — blue
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute — location 0, 3 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // color attribute — location 1, 3 floats, offset 3 floats
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Initial framebuffer
    CreateFramebuffer(1280, 720);
}

void Application::RendererShutdown() {
    DestroyFramebuffer();
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }
}

void Application::RenderScene() {
    m_Shader.Bind();
    m_Shader.SetFloat("u_Time", m_Time);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    m_Shader.Unbind();
}

// ── Framebuffer ───────────────────────────────────────────────────────────────

void Application::CreateFramebuffer(int width, int height) {
    m_FBOWidth  = width;
    m_FBOHeight = height;

    // Color texture
    glGenTextures(1, &m_FBOColorTex);
    glBindTexture(GL_TEXTURE_2D, m_FBOColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &m_FBODepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_FBODepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Framebuffer object
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_FBOColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_FBODepthRBO);

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
    if (width  == m_FBOWidth &&
        height == m_FBOHeight) return;
    if (width  <= 0 || height <= 0) return;

    DestroyFramebuffer();
    CreateFramebuffer(width, height);
}

// ── ImGui ─────────────────────────────────────────────────────────────────────

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

void Application::DrawEditorUI() {
    // ── Fullscreen dockspace ──────────────────────────────────────────────────
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspace_flags =
        ImGuiWindowFlags_NoDocking        |
        ImGuiWindowFlags_NoTitleBar       |
        ImGuiWindowFlags_NoCollapse       |
        ImGuiWindowFlags_NoResize         |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus       |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::Begin("##DockSpace", nullptr, dockspace_flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(m_Window, true);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();

    // ── Hierarchy ─────────────────────────────────────────────────────────────
    ImGui::Begin("Hierarchy");
    ImGui::Text("(empty scene)");
    ImGui::End();

    // ── Inspector ─────────────────────────────────────────────────────────────
    ImGui::Begin("Inspector");
    ImGui::Text("u_Time: %.2f", m_Time);
    ImGui::Text("FBO: %d x %d", m_FBOWidth, m_FBOHeight);
    ImGui::End();

    // ── Viewport ──────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    ResizeFramebufferIfNeeded((int)viewportSize.x, (int)viewportSize.y);

    // Display FBO color texture — flip UV vertically (OpenGL origin is bottom-left)
    ImGui::Image(
        (ImTextureID)(intptr_t)m_FBOColorTex,
        viewportSize,
        ImVec2(0, 1),   // uv0 — top-left in ImGui = bottom-left in GL
        ImVec2(1, 0)    // uv1 — bottom-right in ImGui = top-right in GL
    );

    ImGui::End();
    ImGui::PopStyleVar();

    // ── Console ───────────────────────────────────────────────────────────────
    ImGui::Begin("Console");
    ImGui::Text("[Nova] Engine running. Time: %.2f", m_Time);
    ImGui::End();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        ProcessInput();

        m_Time = (float)glfwGetTime();

        // 1. Render scene into FBO
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, m_FBOWidth, m_FBOHeight);
        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderScene();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Clear main window
        int w, h;
        glfwGetFramebufferSize(m_Window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 3. Draw editor UI (includes viewport image from FBO)
        ImGuiBeginFrame();
        DrawEditorUI();
        ImGuiEndFrame();

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

} // namespace Nova