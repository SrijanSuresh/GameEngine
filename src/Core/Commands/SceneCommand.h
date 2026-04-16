#pragma once

#include "Core/ECS/Components.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Nova {

enum class SceneCommandType {
    CreateEntity,
    DeleteEntity,
    RenameEntity,
    SetPosition,
    SetRotation,
    SetScale,
    SetColor,
    SetPrimitive,
    ApplyMaterialPrompt,
    ClearMaterial
};

struct EntitySnapshot {
    std::string   Name;
    PrimitiveType Primitive = PrimitiveType::Triangle;
    glm::vec3     Position  = { 0.0f, 0.0f, 0.0f };
    glm::vec3     Rotation  = { 0.0f, 0.0f, 0.0f };
    glm::vec3     Scale     = { 1.0f, 1.0f, 1.0f };
    glm::vec4     Tint      = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string   MaterialPrompt;
    bool          HasGeneratedMaterial = false;
};

struct SceneCommand {
    SceneCommandType            Type = SceneCommandType::CreateEntity;
    std::string                 TargetName;
    std::string                 NameValue;
    std::string                 TextValue;
    glm::vec3                   Vec3Value = { 0.0f, 0.0f, 0.0f };
    glm::vec4                   Vec4Value = { 1.0f, 1.0f, 1.0f, 1.0f };
    PrimitiveType               PrimitiveValue = PrimitiveType::Triangle;
    std::optional<EntitySnapshot> Snapshot;
};

struct CommandExecutionResult {
    bool                       Success = false;
    bool                       RecordHistory = false;
    std::string                Status;
    std::optional<SceneCommand> UndoCommand;
    std::optional<SceneCommand> RedoCommand;
};

struct ParsedCommandResult {
    bool                       Success = false;
    std::string                Status;
    std::optional<SceneCommand> Command;
};

} // namespace Nova
