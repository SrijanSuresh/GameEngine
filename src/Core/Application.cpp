#include "Application.h"

#include "Commands/SceneCommandParser.h"
#include "Generative/ShaderGenerator.h"
#include "Serialization/SceneSerializer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace Nova {

namespace {

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
    v_Color = a_Color;
    v_WorldPos = vec3(u_Model * vec4(a_Position, 1.0));
    v_UV = a_Position.xy + 0.5;
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
)";

static const char* s_DefaultFragSrc = R"(
#version 330 core
in vec3 v_Color;
in vec2 v_UV;
out vec4 FragColor;
uniform float u_Time;
uniform vec4 u_TintColor;
void main() {
    float pulse = 0.5 + 0.5 * sin(u_Time * 2.0);
    FragColor = vec4(v_Color * pulse, 1.0) * u_TintColor;
}
)";

void GLFWErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

void FramebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
}

ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

const char* PrimitiveDisplayName(PrimitiveType primitive) {
    return (primitive == PrimitiveType::Quad) ? "Quad" : "Triangle";
}

} // namespace

Application::Application(int width, int height, const char* title)
    : m_Width(width), m_Height(height), m_Title(title) {
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
    m_Camera = Camera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
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

void Application::RendererInit() {
    m_DefaultShader = Shader(s_DefaultVertSrc, s_DefaultFragSrc);
    CreateFramebuffer(1280, 720);

    SceneCommand bootstrap;
    bootstrap.Type = SceneCommandType::CreateEntity;
    bootstrap.NameValue = "Triangle";
    bootstrap.PrimitiveValue = PrimitiveType::Triangle;
    ExecuteSceneCommand(bootstrap, false);
}

void Application::RendererShutdown() {
    DestroyFramebuffer();
    ClearScene();
}

entt::entity Application::CreatePrimitiveEntity(PrimitiveType primitive, const std::string& name) {
    GLuint vao = 0;
    GLuint vbo = 0;

    if (primitive == PrimitiveType::Quad) {
        static constexpr float kQuadVertices[] = {
            -0.5f,  0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
            -0.5f, -0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
             0.5f, -0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
            -0.5f,  0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
             0.5f, -0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
             0.5f,  0.5f, 0.0f,  0.8f, 0.8f, 0.9f,
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        entt::entity entity = m_Scene.CreateEntity(name);
        auto& registry = m_Scene.GetRegistry();
        registry.emplace<MeshRendererComponent>(entity, vao, vbo, 6);
        registry.emplace<PrimitiveComponent>(entity, primitive);
        registry.emplace<ColorComponent>(entity, glm::vec4(1.0f));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return entity;
    }

    static constexpr float kTriangleVertices[] = {
         0.0f,  0.5f,  0.0f,  1.0f, 0.2f, 0.3f,
        -0.5f, -0.5f,  0.0f,  0.2f, 0.8f, 0.4f,
         0.5f, -0.5f,  0.0f,  0.2f, 0.4f, 1.0f,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kTriangleVertices), kTriangleVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    entt::entity entity = m_Scene.CreateEntity(name);
    auto& registry = m_Scene.GetRegistry();
    registry.emplace<MeshRendererComponent>(entity, vao, vbo, 3);
    registry.emplace<PrimitiveComponent>(entity, primitive);
    registry.emplace<ColorComponent>(entity, glm::vec4(1.0f));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return entity;
}

void Application::DestroyRuntimeEntity(entt::entity entity) {
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

std::string Application::MakeUniqueEntityName(const std::string& baseName) const {
    std::string candidate = baseName.empty() ? "Entity" : baseName;
    if (m_Scene.FindEntityByName(candidate) == entt::null)
        return candidate;

    int suffix = m_EntityCounter;
    while (m_Scene.FindEntityByName(candidate + " " + std::to_string(suffix)) != entt::null)
        ++suffix;

    return candidate + " " + std::to_string(suffix);
}

EntitySnapshot Application::CaptureEntitySnapshot(entt::entity entity) const {
    const auto& registry = m_Scene.GetRegistry();
    EntitySnapshot snapshot;
    snapshot.Name = m_Scene.GetEntityName(entity);

    if (registry.all_of<PrimitiveComponent>(entity))
        snapshot.Primitive = registry.get<PrimitiveComponent>(entity).Type;
    if (registry.all_of<TransformComponent>(entity)) {
        const auto& transform = registry.get<TransformComponent>(entity);
        snapshot.Position = transform.Position;
        snapshot.Rotation = transform.Rotation;
        snapshot.Scale = transform.Scale;
    }
    if (registry.all_of<ColorComponent>(entity))
        snapshot.Tint = registry.get<ColorComponent>(entity).Tint;
    if (registry.all_of<MaterialComponent>(entity)) {
        const auto& material = registry.get<MaterialComponent>(entity);
        snapshot.MaterialPrompt = material.Prompt;
        snapshot.HasGeneratedMaterial = material.IsGenerated && !material.Prompt.empty();
    }

    return snapshot;
}

std::vector<EntitySnapshot> Application::CaptureSceneSnapshots() const {
    std::vector<EntitySnapshot> snapshots;
    auto view = m_Scene.GetRegistry().view<TagComponent>();
    for (auto entity : view)
        snapshots.push_back(CaptureEntitySnapshot(entity));
    return snapshots;
}

entt::entity Application::RestoreEntityFromSnapshot(const EntitySnapshot& snapshot) {
    entt::entity entity = CreatePrimitiveEntity(snapshot.Primitive, snapshot.Name);
    auto& registry = m_Scene.GetRegistry();
    auto& transform = registry.get<TransformComponent>(entity);
    transform.Position = snapshot.Position;
    transform.Rotation = snapshot.Rotation;
    transform.Scale = snapshot.Scale;
    registry.get<ColorComponent>(entity).Tint = snapshot.Tint;

    if (snapshot.HasGeneratedMaterial && !snapshot.MaterialPrompt.empty())
        GenerateShaderForEntity(entity, snapshot.MaterialPrompt);

    return entity;
}

void Application::ClearScene() {
    auto view = m_Scene.GetRegistry().view<TagComponent>();
    std::vector<entt::entity> entities(view.begin(), view.end());
    for (entt::entity entity : entities)
        DestroyRuntimeEntity(entity);
    m_SelectedEntity = entt::null;
}

bool Application::GenerateShaderForEntity(entt::entity entity, const std::string& prompt) {
    m_LastShaderError.clear();

    std::string vertSrc = ShaderGenerator::GetVertexSource();
    std::string fragSrc = ShaderGenerator::Generate(prompt);
    auto newShader = std::make_shared<Shader>(vertSrc.c_str(), fragSrc.c_str());

    if (!newShader->IsValid()) {
        m_LastShaderError = "Shader compilation failed. Check console for GLSL errors.";
        return false;
    }

    auto& registry = m_Scene.GetRegistry();
    if (!registry.all_of<MaterialComponent>(entity))
        registry.emplace<MaterialComponent>(entity);

    auto& material = registry.get<MaterialComponent>(entity);
    material.Prompt = prompt;
    material.Mat = newShader;
    material.IsGenerated = true;
    return true;
}

void Application::ClearMaterialForEntity(entt::entity entity) {
    auto& registry = m_Scene.GetRegistry();
    if (registry.all_of<MaterialComponent>(entity))
        registry.remove<MaterialComponent>(entity);
}

CommandExecutionResult Application::ExecuteSceneCommand(const SceneCommand& command, bool trackHistory) {
    CommandExecutionResult result;
    auto& registry = m_Scene.GetRegistry();

    auto resolveTarget = [&]() -> entt::entity {
        if (command.TargetName.empty())
            return entt::null;
        return m_Scene.FindEntityByName(command.TargetName);
    };

    switch (command.Type) {
    case SceneCommandType::CreateEntity: {
        if (command.Snapshot) {
            entt::entity entity = RestoreEntityFromSnapshot(*command.Snapshot);
            m_SelectedEntity = entity;

            SceneCommand undo;
            undo.Type = SceneCommandType::DeleteEntity;
            undo.TargetName = command.Snapshot->Name;

            result.Success = true;
            result.RecordHistory = trackHistory;
            result.Status = "Restored \"" + command.Snapshot->Name + "\".";
            result.UndoCommand = undo;
            result.RedoCommand = command;
            return result;
        }

        std::string baseName = command.NameValue.empty() ? PrimitiveDisplayName(command.PrimitiveValue) : command.NameValue;
        std::string uniqueName = MakeUniqueEntityName(baseName);
        entt::entity entity = CreatePrimitiveEntity(command.PrimitiveValue, uniqueName);
        m_SelectedEntity = entity;

        SceneCommand undo;
        undo.Type = SceneCommandType::DeleteEntity;
        undo.TargetName = uniqueName;

        SceneCommand redo = command;
        redo.NameValue = uniqueName;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Created " + std::string(PrimitiveDisplayName(command.PrimitiveValue)) + " \"" + uniqueName + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = redo;
        return result;
    }

    case SceneCommandType::DeleteEntity: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        EntitySnapshot snapshot = CaptureEntitySnapshot(entity);
        DestroyRuntimeEntity(entity);
        if (m_SelectedEntity == entity)
            m_SelectedEntity = entt::null;

        SceneCommand undo;
        undo.Type = SceneCommandType::CreateEntity;
        undo.NameValue = snapshot.Name;
        undo.PrimitiveValue = snapshot.Primitive;
        undo.Snapshot = snapshot;

        SceneCommand redo;
        redo.Type = SceneCommandType::DeleteEntity;
        redo.TargetName = snapshot.Name;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Deleted \"" + snapshot.Name + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = redo;
        return result;
    }

    case SceneCommandType::RenameEntity: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        const std::string oldName = m_Scene.GetEntityName(entity);
        const std::string newName = MakeUniqueEntityName(command.NameValue);
        registry.get<TagComponent>(entity).Name = newName;

        SceneCommand undo;
        undo.Type = SceneCommandType::RenameEntity;
        undo.TargetName = newName;
        undo.NameValue = oldName;

        SceneCommand redo = command;
        redo.TargetName = oldName;
        redo.NameValue = newName;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Renamed \"" + oldName + "\" to \"" + newName + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = redo;
        return result;
    }

    case SceneCommandType::SetPosition:
    case SceneCommandType::SetRotation:
    case SceneCommandType::SetScale: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        auto& transform = registry.get<TransformComponent>(entity);
        glm::vec3* targetVector = nullptr;
        const char* label = "";
        if (command.Type == SceneCommandType::SetPosition) {
            targetVector = &transform.Position;
            label = "position";
        } else if (command.Type == SceneCommandType::SetRotation) {
            targetVector = &transform.Rotation;
            label = "rotation";
        } else {
            targetVector = &transform.Scale;
            label = "scale";
        }

        SceneCommand undo = command;
        undo.Vec3Value = *targetVector;
        *targetVector = command.Vec3Value;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Updated " + std::string(label) + " for \"" + command.TargetName + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = command;
        return result;
    }

    case SceneCommandType::SetColor: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        auto& color = registry.get<ColorComponent>(entity);
        SceneCommand undo = command;
        undo.Vec4Value = color.Tint;
        color.Tint = command.Vec4Value;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Updated color for \"" + command.TargetName + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = command;
        return result;
    }

    case SceneCommandType::SetPrimitive: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        EntitySnapshot previous = CaptureEntitySnapshot(entity);
        if (previous.Primitive == command.PrimitiveValue)
            return { true, false, "Primitive unchanged.", std::nullopt, std::nullopt };

        EntitySnapshot replacement = previous;
        replacement.Primitive = command.PrimitiveValue;
        DestroyRuntimeEntity(entity);
        entt::entity restored = RestoreEntityFromSnapshot(replacement);
        m_SelectedEntity = restored;

        SceneCommand undo;
        undo.Type = SceneCommandType::SetPrimitive;
        undo.TargetName = replacement.Name;
        undo.PrimitiveValue = previous.Primitive;

        SceneCommand redo = command;
        redo.TargetName = replacement.Name;

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Switched \"" + replacement.Name + "\" to " + std::string(PrimitiveDisplayName(command.PrimitiveValue)) + ".";
        result.UndoCommand = undo;
        result.RedoCommand = redo;
        return result;
    }

    case SceneCommandType::ApplyMaterialPrompt: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        EntitySnapshot previous = CaptureEntitySnapshot(entity);
        if (!GenerateShaderForEntity(entity, command.TextValue))
            return { false, false, m_LastShaderError, std::nullopt, std::nullopt };

        SceneCommand undo;
        undo.TargetName = command.TargetName;
        if (previous.HasGeneratedMaterial) {
            undo.Type = SceneCommandType::ApplyMaterialPrompt;
            undo.TextValue = previous.MaterialPrompt;
        } else {
            undo.Type = SceneCommandType::ClearMaterial;
        }

        result.Success = true;
        result.RecordHistory = trackHistory;
        result.Status = "Applied material prompt \"" + command.TextValue + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = command;
        return result;
    }

    case SceneCommandType::ClearMaterial: {
        entt::entity entity = resolveTarget();
        if (entity == entt::null)
            return { false, false, "Couldn't find an entity named \"" + command.TargetName + "\".", std::nullopt, std::nullopt };

        EntitySnapshot previous = CaptureEntitySnapshot(entity);
        ClearMaterialForEntity(entity);

        SceneCommand undo;
        undo.TargetName = command.TargetName;
        if (previous.HasGeneratedMaterial) {
            undo.Type = SceneCommandType::ApplyMaterialPrompt;
            undo.TextValue = previous.MaterialPrompt;
        } else {
            undo.Type = SceneCommandType::ClearMaterial;
        }

        result.Success = true;
        result.RecordHistory = trackHistory && previous.HasGeneratedMaterial;
        result.Status = "Cleared material for \"" + command.TargetName + "\".";
        result.UndoCommand = undo;
        result.RedoCommand = command;
        return result;
    }
    }

    return { false, false, "Unsupported command.", std::nullopt, std::nullopt };
}

bool Application::ExecutePromptInput(const std::string& input) {
    if (input == "save scene")
        return SaveScene(m_DefaultScenePath);
    if (input == "load scene")
        return LoadScene(m_DefaultScenePath);
    if (input == "undo") {
        UndoLastCommand();
        return true;
    }
    if (input == "redo") {
        RedoLastCommand();
        return true;
    }

    const std::string selectedName =
        (m_SelectedEntity != entt::null && m_Scene.IsValid(m_SelectedEntity))
            ? m_Scene.GetEntityName(m_SelectedEntity)
            : "";

    ParsedCommandResult parsed = SceneCommandParser::Parse(input, selectedName);
    if (!parsed.Success) {
        m_CommandStatus = parsed.Status;
        m_CommandHadError = true;
        return false;
    }

    CommandExecutionResult executed = ExecuteSceneCommand(*parsed.Command, true);
    m_CommandStatus = executed.Status;
    m_CommandHadError = !executed.Success;

    if (executed.Success && executed.RecordHistory && executed.UndoCommand && executed.RedoCommand)
        m_History.Record(*executed.UndoCommand, *executed.RedoCommand);

    return executed.Success;
}

void Application::UndoLastCommand() {
    if (!m_History.CanUndo()) {
        m_CommandStatus = "Nothing to undo.";
        m_CommandHadError = true;
        return;
    }

    CommandHistoryEntry entry = m_History.PopUndo();
    CommandExecutionResult result = ExecuteSceneCommand(entry.Undo, false);
    if (result.Success) {
        m_History.PushRedo(entry);
        m_CommandStatus = "Undid the last change.";
        m_CommandHadError = false;
    } else {
        m_CommandStatus = result.Status;
        m_CommandHadError = true;
    }
}

void Application::RedoLastCommand() {
    if (!m_History.CanRedo()) {
        m_CommandStatus = "Nothing to redo.";
        m_CommandHadError = true;
        return;
    }

    CommandHistoryEntry entry = m_History.PopRedo();
    CommandExecutionResult result = ExecuteSceneCommand(entry.Redo, false);
    if (result.Success) {
        m_History.PushUndo(entry);
        m_CommandStatus = "Redid the last change.";
        m_CommandHadError = false;
    } else {
        m_CommandStatus = result.Status;
        m_CommandHadError = true;
    }
}

bool Application::SaveScene(const std::string& path) {
    std::string errorMessage;
    const bool success = SceneSerializer::Save(path, CaptureSceneSnapshots(), errorMessage);
    m_CommandStatus = success ? "Saved scene to " + path : errorMessage;
    m_CommandHadError = !success;
    return success;
}

bool Application::LoadScene(const std::string& path) {
    std::vector<EntitySnapshot> snapshots;
    std::string errorMessage;
    if (!SceneSerializer::Load(path, snapshots, errorMessage)) {
        m_CommandStatus = errorMessage;
        m_CommandHadError = true;
        return false;
    }

    ClearScene();
    for (const EntitySnapshot& snapshot : snapshots)
        RestoreEntityFromSnapshot(snapshot);

    auto view = m_Scene.GetRegistry().view<TagComponent>();
    m_SelectedEntity = view.empty() ? entt::null : *view.begin();
    m_History.Clear();
    m_CommandStatus = "Loaded scene from " + path + ".";
    m_CommandHadError = false;
    return true;
}

void Application::RenderScene() {
    auto view = m_Scene.GetRegistry().view<MeshRendererComponent, TransformComponent, ColorComponent>();
    for (auto entity : view) {
        auto& mesh = view.get<MeshRendererComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        auto& color = view.get<ColorComponent>(entity);

        Shader* shader = &m_DefaultShader;
        if (m_Scene.GetRegistry().all_of<MaterialComponent>(entity)) {
            auto& material = m_Scene.GetRegistry().get<MaterialComponent>(entity);
            if (material.IsGenerated && material.Mat && material.Mat->IsValid())
                shader = material.Mat.get();
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.Position);
        model = glm::rotate(model, glm::radians(transform.Rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(transform.Rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(transform.Rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, transform.Scale);

        shader->Bind();
        shader->SetFloat("u_Time", m_Time);
        shader->SetMat4("u_Model", model);
        shader->SetMat4("u_View", m_Camera.GetViewMatrix());
        shader->SetMat4("u_Projection", m_Camera.GetProjectionMatrix());
        shader->SetVec4("u_TintColor", color.Tint);
        glBindVertexArray(mesh.VAO);
        glDrawArrays(GL_TRIANGLES, 0, mesh.VertexCount);
        glBindVertexArray(0);
        shader->Unbind();
    }
}

void Application::CreateFramebuffer(int width, int height) {
    m_FBOWidth = width;
    m_FBOHeight = height;

    glGenTextures(1, &m_FBOColorTex);
    glBindTexture(GL_TEXTURE_2D, m_FBOColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &m_FBODepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_FBODepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_FBOColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_FBODepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[Nova] Framebuffer incomplete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Application::DestroyFramebuffer() {
    if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_FBOColorTex) { glDeleteTextures(1, &m_FBOColorTex); m_FBOColorTex = 0; }
    if (m_FBODepthRBO) { glDeleteRenderbuffers(1, &m_FBODepthRBO); m_FBODepthRBO = 0; }
}

void Application::ResizeFramebufferIfNeeded(int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    if (width == m_FBOWidth && height == m_FBOHeight)
        return;

    DestroyFramebuffer();
    CreateFramebuffer(width, height);
}

void Application::ImGuiInit() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 16.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 10.0f;
    style.TabRounding = 10.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.IndentSpacing = 18.0f;

    ApplyTheme();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

void Application::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    const float t = m_ThemeBlend;

    const ImVec4 inkWindow = ImVec4(0.08f, 0.10f, 0.14f, 0.96f);
    const ImVec4 mistWindow = ImVec4(0.95f, 0.96f, 0.98f, 0.96f);
    const ImVec4 inkPanel = ImVec4(0.11f, 0.14f, 0.19f, 0.96f);
    const ImVec4 mistPanel = ImVec4(0.98f, 0.985f, 0.995f, 0.97f);
    const ImVec4 inkSurface = ImVec4(0.15f, 0.18f, 0.24f, 1.0f);
    const ImVec4 mistSurface = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    const ImVec4 inkAccent = ImVec4(0.37f, 0.67f, 0.95f, 1.0f);
    const ImVec4 mistAccent = ImVec4(0.18f, 0.48f, 0.91f, 1.0f);
    const ImVec4 inkHover = ImVec4(0.45f, 0.74f, 0.98f, 1.0f);
    const ImVec4 mistHover = ImVec4(0.29f, 0.58f, 0.95f, 1.0f);
    const ImVec4 inkText = ImVec4(0.92f, 0.95f, 0.99f, 1.0f);
    const ImVec4 mistText = ImVec4(0.16f, 0.20f, 0.28f, 1.0f);
    const ImVec4 inkMuted = ImVec4(0.59f, 0.67f, 0.79f, 1.0f);
    const ImVec4 mistMuted = ImVec4(0.45f, 0.51f, 0.61f, 1.0f);
    const ImVec4 inkBorder = ImVec4(0.27f, 0.35f, 0.47f, 0.36f);
    const ImVec4 mistBorder = ImVec4(0.54f, 0.63f, 0.76f, 0.22f);

    colors[ImGuiCol_Text] = LerpColor(inkText, mistText, t);
    colors[ImGuiCol_TextDisabled] = LerpColor(inkMuted, mistMuted, t);
    colors[ImGuiCol_WindowBg] = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_ChildBg] = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_PopupBg] = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_Border] = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_FrameBgHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_FrameBgActive] = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_TitleBg] = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_TitleBgActive] = LerpColor(inkPanel, mistPanel, t);
    colors[ImGuiCol_MenuBarBg] = LerpColor(ImVec4(0.06f, 0.08f, 0.12f, 0.98f), ImVec4(0.93f, 0.95f, 0.98f, 0.98f), t);
    colors[ImGuiCol_ScrollbarBg] = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_ScrollbarGrab] = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_ScrollbarGrabHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_ScrollbarGrabActive] = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_CheckMark] = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_SliderGrab] = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_SliderGrabActive] = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_Button] = LerpColor(ImVec4(0.18f, 0.24f, 0.32f, 0.90f), ImVec4(0.86f, 0.90f, 0.96f, 0.95f), t);
    colors[ImGuiCol_ButtonHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_ButtonActive] = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_Header] = LerpColor(ImVec4(0.19f, 0.26f, 0.36f, 0.90f), ImVec4(0.83f, 0.89f, 0.97f, 0.98f), t);
    colors[ImGuiCol_HeaderHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_HeaderActive] = LerpColor(inkHover, mistAccent, t);
    colors[ImGuiCol_Separator] = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_SeparatorHovered] = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_SeparatorActive] = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_ResizeGrip] = LerpColor(inkBorder, mistBorder, t);
    colors[ImGuiCol_ResizeGripHovered] = LerpColor(inkAccent, mistAccent, t);
    colors[ImGuiCol_ResizeGripActive] = LerpColor(inkHover, mistHover, t);
    colors[ImGuiCol_Tab] = LerpColor(ImVec4(0.13f, 0.17f, 0.23f, 0.95f), ImVec4(0.89f, 0.92f, 0.97f, 0.95f), t);
    colors[ImGuiCol_TabHovered] = LerpColor(inkAccent, mistHover, t);
    colors[ImGuiCol_TabActive] = LerpColor(ImVec4(0.17f, 0.23f, 0.32f, 0.98f), ImVec4(0.84f, 0.89f, 0.97f, 0.98f), t);
    colors[ImGuiCol_TabUnfocused] = LerpColor(inkWindow, mistWindow, t);
    colors[ImGuiCol_TabUnfocusedActive] = LerpColor(inkSurface, mistSurface, t);
    colors[ImGuiCol_DockingEmptyBg] = LerpColor(ImVec4(0.05f, 0.07f, 0.10f, 1.0f), ImVec4(0.91f, 0.94f, 0.98f, 1.0f), t);
    colors[ImGuiCol_TextSelectedBg] = LerpColor(ImVec4(0.30f, 0.56f, 0.90f, 0.35f), ImVec4(0.27f, 0.53f, 0.92f, 0.30f), t);
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

void Application::DrawEditorUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags =
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockSpace", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                SaveScene(m_DefaultScenePath);
            if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
                LoadScene(m_DefaultScenePath);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                glfwSetWindowShouldClose(m_Window, true);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_History.CanUndo()))
                UndoLastCommand();
            if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, m_History.CanRedo()))
                RedoLastCommand();
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

    const bool inkActive = (m_Theme == EditorTheme::Ink);
    const bool mistActive = (m_Theme == EditorTheme::Mist);

    if (inkActive)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Ink"))
        m_Theme = EditorTheme::Ink;
    if (inkActive)
        ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);

    if (mistActive)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Mist"))
        m_Theme = EditorTheme::Mist;
    if (mistActive)
        ImGui::PopStyleColor();

    ImGui::PopStyleVar(2);
}

void Application::DrawHierarchyPanel() {
    ImGui::Begin("Hierarchy");

    if (ImGui::Button("+ Add")) {
        SceneCommand command;
        command.Type = SceneCommandType::CreateEntity;
        command.PrimitiveValue = PrimitiveType::Triangle;
        command.NameValue = "Triangle";
        CommandExecutionResult result = ExecuteSceneCommand(command, true);
        if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
            m_History.Record(*result.UndoCommand, *result.RedoCommand);
        m_CommandStatus = result.Status;
        m_CommandHadError = !result.Success;
    }

    ImGui::SameLine();
    if (ImGui::Button("+ Quad")) {
        SceneCommand command;
        command.Type = SceneCommandType::CreateEntity;
        command.PrimitiveValue = PrimitiveType::Quad;
        command.NameValue = "Quad";
        CommandExecutionResult result = ExecuteSceneCommand(command, true);
        if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
            m_History.Record(*result.UndoCommand, *result.RedoCommand);
        m_CommandStatus = result.Status;
        m_CommandHadError = !result.Success;
    }

    ImGui::Separator();

    auto view = m_Scene.GetRegistry().view<TagComponent, PrimitiveComponent>();
    for (auto entity : view) {
        const auto& tag = view.get<TagComponent>(entity);
        const auto& primitive = view.get<PrimitiveComponent>(entity);
        const std::string label = std::string(PrimitiveDisplayName(primitive.Type)) + "  " + tag.Name;
        if (ImGui::Selectable(label.c_str(), entity == m_SelectedEntity))
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
    const std::string selectedName = m_Scene.GetEntityName(m_SelectedEntity);

    if (registry.all_of<TagComponent>(m_SelectedEntity)) {
        auto& tag = registry.get<TagComponent>(m_SelectedEntity);
        char nameBuf[256] = {};
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", tag.Name.c_str());
        ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
        if (ImGui::IsItemDeactivatedAfterEdit() && std::string(nameBuf) != tag.Name) {
            SceneCommand command;
            command.Type = SceneCommandType::RenameEntity;
            command.TargetName = selectedName;
            command.NameValue = nameBuf;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }
    }

    ImGui::Separator();

    if (registry.all_of<PrimitiveComponent>(m_SelectedEntity)) {
        int currentPrimitive = registry.get<PrimitiveComponent>(m_SelectedEntity).Type == PrimitiveType::Quad ? 1 : 0;
        const char* primitiveNames[] = { "Triangle", "Quad" };
        ImGui::Combo("Primitive", &currentPrimitive, primitiveNames, IM_ARRAYSIZE(primitiveNames));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SceneCommand command;
            command.Type = SceneCommandType::SetPrimitive;
            command.TargetName = selectedName;
            command.PrimitiveValue = (currentPrimitive == 1) ? PrimitiveType::Quad : PrimitiveType::Triangle;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }
    }

    if (registry.all_of<ColorComponent>(m_SelectedEntity)) {
        glm::vec4 tint = registry.get<ColorComponent>(m_SelectedEntity).Tint;
        ImGui::ColorEdit4("Tint", &tint.x);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SceneCommand command;
            command.Type = SceneCommandType::SetColor;
            command.TargetName = selectedName;
            command.Vec4Value = tint;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }
    }

    ImGui::Separator();

    if (registry.all_of<TransformComponent>(m_SelectedEntity)) {
        auto& transform = registry.get<TransformComponent>(m_SelectedEntity);

        glm::vec3 position = transform.Position;
        ImGui::DragFloat3("Position", &position.x, 0.1f);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SceneCommand command;
            command.Type = SceneCommandType::SetPosition;
            command.TargetName = selectedName;
            command.Vec3Value = position;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }

        glm::vec3 rotation = transform.Rotation;
        ImGui::DragFloat3("Rotation", &rotation.x, 1.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SceneCommand command;
            command.Type = SceneCommandType::SetRotation;
            command.TargetName = selectedName;
            command.Vec3Value = rotation;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }

        glm::vec3 scale = transform.Scale;
        ImGui::DragFloat3("Scale", &scale.x, 0.1f);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            SceneCommand command;
            command.Type = SceneCommandType::SetScale;
            command.TargetName = selectedName;
            command.Vec3Value = scale;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::string currentPrompt;
        if (registry.all_of<MaterialComponent>(m_SelectedEntity))
            currentPrompt = registry.get<MaterialComponent>(m_SelectedEntity).Prompt;

        static char promptBuf[256] = "";
        static entt::entity lastInspected = entt::null;
        if (lastInspected != m_SelectedEntity) {
            std::snprintf(promptBuf, sizeof(promptBuf), "%s", currentPrompt.c_str());
            lastInspected = m_SelectedEntity;
            m_LastShaderError.clear();
        }

        ImGui::TextDisabled("Keywords: galaxy fire ocean nebula cyberpunk");
        ImGui::TextDisabled("          lava aurora glitch plasma void");
        ImGui::SetNextItemWidth(-1.0f);
        bool pressedEnter = ImGui::InputText("##prompt", promptBuf, sizeof(promptBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        bool clickedGenerate = ImGui::Button("Generate");

        if (pressedEnter || clickedGenerate) {
            SceneCommand command;
            command.Type = SceneCommandType::ApplyMaterialPrompt;
            command.TargetName = selectedName;
            command.TextValue = promptBuf;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
        }

        if (!m_LastShaderError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", m_LastShaderError.c_str());
            ImGui::PopStyleColor();
        } else if (registry.all_of<MaterialComponent>(m_SelectedEntity)) {
            auto& material = registry.get<MaterialComponent>(m_SelectedEntity);
            if (material.IsGenerated)
                ImGui::Text("Shader active: %s", material.Prompt.c_str());
        }

        if (ImGui::Button("Clear Material")) {
            SceneCommand command;
            command.Type = SceneCommandType::ClearMaterial;
            command.TargetName = selectedName;
            CommandExecutionResult result = ExecuteSceneCommand(command, true);
            if (result.Success && result.RecordHistory && result.UndoCommand && result.RedoCommand)
                m_History.Record(*result.UndoCommand, *result.RedoCommand);
            m_CommandStatus = result.Status;
            m_CommandHadError = !result.Success;
            std::memset(promptBuf, 0, sizeof(promptBuf));
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

    ImGui::Image((ImTextureID)(intptr_t)m_FBOColorTex, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();
    ImGui::PopStyleVar();
}

void Application::DrawConsolePanel() {
    ImGui::Begin("Console");
    const float fps = (m_DeltaTime > 0.0f) ? (1.0f / m_DeltaTime) : 0.0f;
    ImGui::Text("[Nova] Time: %.2f | FPS: %.0f", m_Time, fps);
    ImGui::Text("[Nova] Camera: (%.2f, %.2f, %.2f)", m_Camera.GetPosition().x, m_Camera.GetPosition().y, m_Camera.GetPosition().z);
    ImGui::Text("[Nova] Entities: %d", (int)m_Scene.GetRegistry().storage<entt::entity>().size());
    ImGui::Text("[Nova] History: %s / %s", m_History.CanUndo() ? "undo" : "-", m_History.CanRedo() ? "redo" : "-");
    if (!m_CommandStatus.empty())
        ImGui::TextWrapped("[Nova] %s", m_CommandStatus.c_str());
    ImGui::End();
}

void Application::DrawPromptPanel() {
    ImGui::Begin("Prompt");

    ImGui::TextWrapped("Prompt input now routes through structured SceneCommands. That keeps Nova readable, undoable, and ready for AI later.");
    ImGui::TextDisabled("Examples:");
    ImGui::BulletText("create quad named Floor");
    ImGui::BulletText("move selected to 1 0 0");
    ImGui::BulletText("change color of Floor to blue");
    ImGui::BulletText("material aurora glitch");
    ImGui::BulletText("save scene");
    ImGui::BulletText("undo");
    ImGui::Spacing();

    static char commandBuf[256] = "";
    ImGui::SetNextItemWidth(-1.0f);
    const bool pressedEnter = ImGui::InputText("##scene-command", commandBuf, sizeof(commandBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool clickedRun = ImGui::Button("Run Command");

    if (pressedEnter || clickedRun)
        ExecutePromptInput(commandBuf);

    if (!m_CommandStatus.empty()) {
        const ImVec4 color = m_CommandHadError ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.35f, 1.0f, 0.55f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", m_CommandStatus.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_Window)) {
        m_Time = (float)glfwGetTime();
        m_DeltaTime = m_Time - m_LastTime;
        m_LastTime = m_Time;

        ProcessInput();
        m_Camera.OnUpdate(m_Window, m_DeltaTime);

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, m_FBOWidth, m_FBOHeight);
        glEnable(GL_DEPTH_TEST);
        const ImVec4 sceneClear = LerpColor(ImVec4(0.12f, 0.13f, 0.17f, 1.0f), ImVec4(0.83f, 0.88f, 0.95f, 1.0f), m_ThemeBlend);
        glClearColor(sceneClear.x, sceneClear.y, sceneClear.z, sceneClear.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderScene();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        int w, h;
        glfwGetFramebufferSize(m_Window, &w, &h);
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        const ImVec4 appClear = LerpColor(ImVec4(0.06f, 0.08f, 0.11f, 1.0f), ImVec4(0.92f, 0.95f, 0.985f, 1.0f), m_ThemeBlend);
        glClearColor(appClear.x, appClear.y, appClear.z, appClear.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGuiBeginFrame();
        DrawEditorUI();
        ImGuiEndFrame();

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

} // namespace Nova
