#include "SceneCommandParser.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace Nova {

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool TryParseVec3(const std::string& text, glm::vec3& outValue) {
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream stream(normalized);
    stream >> outValue.x >> outValue.y >> outValue.z;
    return !stream.fail();
}

std::optional<glm::vec4> ParseColor(const std::string& text) {
    static const std::unordered_map<std::string, glm::vec4> kNamedColors = {
        { "red",    { 1.0f, 0.25f, 0.25f, 1.0f } },
        { "green",  { 0.25f, 0.85f, 0.45f, 1.0f } },
        { "blue",   { 0.25f, 0.55f, 1.0f, 1.0f } },
        { "yellow", { 0.98f, 0.85f, 0.25f, 1.0f } },
        { "orange", { 1.0f, 0.58f, 0.20f, 1.0f } },
        { "purple", { 0.69f, 0.42f, 0.98f, 1.0f } },
        { "pink",   { 1.0f, 0.49f, 0.74f, 1.0f } },
        { "white",  { 1.0f, 1.0f, 1.0f, 1.0f } },
        { "black",  { 0.08f, 0.08f, 0.10f, 1.0f } },
        { "gray",   { 0.55f, 0.58f, 0.64f, 1.0f } },
        { "grey",   { 0.55f, 0.58f, 0.64f, 1.0f } },
        { "cyan",   { 0.20f, 0.88f, 0.92f, 1.0f } }
    };

    const std::string normalized = ToLower(Trim(text));
    const auto it = kNamedColors.find(normalized);
    if (it != kNamedColors.end())
        return it->second;

    glm::vec3 rgb;
    if (TryParseVec3(normalized, rgb))
        return glm::vec4(rgb, 1.0f);

    return std::nullopt;
}

ParsedCommandResult RequireSelection(const std::string& selectedEntityName, const SceneCommand& command) {
    if (selectedEntityName.empty()) {
        return { false, "Select an entity first.", std::nullopt };
    }

    SceneCommand resolved = command;
    resolved.TargetName = selectedEntityName;
    return { true, "", resolved };
}

} // namespace

ParsedCommandResult SceneCommandParser::Parse(const std::string& input, const std::string& selectedEntityName) {
    const std::string trimmed = Trim(input);
    const std::string lower = ToLower(trimmed);

    if (trimmed.empty())
        return { false, "Type a command first.", std::nullopt };

    if ((StartsWith(lower, "create ") || StartsWith(lower, "add ") || StartsWith(lower, "spawn ")) &&
        (lower.find("triangle") != std::string::npos || lower.find("quad") != std::string::npos)) {
        SceneCommand command;
        command.Type = SceneCommandType::CreateEntity;
        command.PrimitiveValue = (lower.find("quad") != std::string::npos) ? PrimitiveType::Quad : PrimitiveType::Triangle;

        const size_t namedPos = lower.find(" named ");
        if (namedPos != std::string::npos)
            command.NameValue = Trim(trimmed.substr(namedPos + 7));

        return { true, "", command };
    }

    if (lower == "delete selected") {
        SceneCommand command;
        command.Type = SceneCommandType::DeleteEntity;
        return RequireSelection(selectedEntityName, command);
    }

    if (StartsWith(lower, "delete ")) {
        SceneCommand command;
        command.Type = SceneCommandType::DeleteEntity;
        command.TargetName = Trim(trimmed.substr(7));
        return { true, "", command };
    }

    if (StartsWith(lower, "rename selected to ")) {
        SceneCommand command;
        command.Type = SceneCommandType::RenameEntity;
        command.NameValue = Trim(trimmed.substr(19));
        return RequireSelection(selectedEntityName, command);
    }

    if (StartsWith(lower, "select ")) {
        return { false, "Selection stays a direct editor action for now. Click in the hierarchy to change selection.", std::nullopt };
    }

    if (StartsWith(lower, "move selected to ")) {
        SceneCommand command;
        command.Type = SceneCommandType::SetPosition;
        if (!TryParseVec3(trimmed.substr(17), command.Vec3Value))
            return { false, "Use three numbers for position, like: move selected to 1 2 0", std::nullopt };
        return RequireSelection(selectedEntityName, command);
    }

    if (StartsWith(lower, "move ")) {
        const size_t toPos = lower.find(" to ");
        if (toPos != std::string::npos) {
            SceneCommand command;
            command.Type = SceneCommandType::SetPosition;
            command.TargetName = Trim(trimmed.substr(5, toPos - 5));
            if (!TryParseVec3(trimmed.substr(toPos + 4), command.Vec3Value))
                return { false, "Use three numbers for position, like: move Player to 1 2 0", std::nullopt };
            return { true, "", command };
        }
    }

    if (StartsWith(lower, "change color of ")) {
        const size_t toPos = lower.find(" to ");
        if (toPos != std::string::npos) {
            SceneCommand command;
            command.Type = SceneCommandType::SetColor;
            command.TargetName = Trim(trimmed.substr(16, toPos - 16));
            auto parsedColor = ParseColor(trimmed.substr(toPos + 4));
            if (!parsedColor)
                return { false, "Use a named color like red or blue, or RGB numbers like 1 0 0.", std::nullopt };
            command.Vec4Value = *parsedColor;
            return { true, "", command };
        }
    }

    if (StartsWith(lower, "set color of ")) {
        const size_t toPos = lower.find(" to ");
        if (toPos != std::string::npos) {
            SceneCommand command;
            command.Type = SceneCommandType::SetColor;
            command.TargetName = Trim(trimmed.substr(13, toPos - 13));
            auto parsedColor = ParseColor(trimmed.substr(toPos + 4));
            if (!parsedColor)
                return { false, "Use a named color like red or blue, or RGB numbers like 1 0 0.", std::nullopt };
            command.Vec4Value = *parsedColor;
            return { true, "", command };
        }
    }

    if (StartsWith(lower, "color selected ")) {
        SceneCommand command;
        command.Type = SceneCommandType::SetColor;
        auto parsedColor = ParseColor(trimmed.substr(15));
        if (!parsedColor)
            return { false, "Use a named color like red or blue, or RGB numbers like 1 0 0.", std::nullopt };
        command.Vec4Value = *parsedColor;
        return RequireSelection(selectedEntityName, command);
    }

    if (StartsWith(lower, "material ")) {
        SceneCommand command;
        command.Type = SceneCommandType::ApplyMaterialPrompt;
        command.TextValue = Trim(trimmed.substr(9));
        if (command.TextValue.empty())
            return { false, "Type a material description after the word material.", std::nullopt };
        return RequireSelection(selectedEntityName, command);
    }

    if (lower == "clear material") {
        SceneCommand command;
        command.Type = SceneCommandType::ClearMaterial;
        return RequireSelection(selectedEntityName, command);
    }

    return {
        false,
        "Unknown command. Try create quad named Floor, move selected to 1 0 0, change color of Floor to blue, or material aurora.",
        std::nullopt
    };
}

} // namespace Nova
