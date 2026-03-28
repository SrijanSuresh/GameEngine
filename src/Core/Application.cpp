#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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
}

void Application::Shutdown() {
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

// ── ImGui ─────────────────────────────────────────────────────────────────────

void Application::ImGuiInit() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Dark theme
    ImGui::StyleColorsDark();

    // Tweak style to feel more like an engine editor
    ImGuiStyle& style = ImGui::GetStyle();
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
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##DockSpace", nullptr, dockspace_flags);
    ImGui::PopStyleVar(3);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene"))  { /* TODO */ }
            if (ImGui::MenuItem("Open Scene")) { /* TODO */ }
            if (ImGui::MenuItem("Save Scene")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(m_Window, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Dockspace
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();

    // ── Hierarchy panel ───────────────────────────────────────────────────────
    ImGui::Begin("Hierarchy");
    ImGui::Text("(empty scene)");
    // future: list ECS entities here
    ImGui::End();

    // ── Inspector panel ───────────────────────────────────────────────────────
    ImGui::Begin("Inspector");
    ImGui::Text("Select an object to inspect.");
    // future: show selected entity components here
    ImGui::End();

    // ── Viewport panel ───────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");
    ImVec2 size = ImGui::GetContentRegionAvail();
    // future: render scene to FBO, display as ImGui::Image here
    ImGui::Text("Viewport — %.0f x %.0f", size.x, size.y);
    ImGui::End();
    ImGui::PopStyleVar();

    // ── Console panel ────────────────────────────────────────────────────────
    ImGui::Begin("Console");
    ImGui::Text("[Nova] Engine started.");
    // future: route log output here
    ImGui::End();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        ProcessInput();

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGuiBeginFrame();
        DrawEditorUI();
        ImGuiEndFrame();

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

} // namespace Nova