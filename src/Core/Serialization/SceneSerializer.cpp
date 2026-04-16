#include "SceneSerializer.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace Nova {

namespace {

std::string PrimitiveToString(PrimitiveType primitive) {
    return (primitive == PrimitiveType::Quad) ? "quad" : "triangle";
}

PrimitiveType PrimitiveFromString(const std::string& value) {
    return (value == "quad") ? PrimitiveType::Quad : PrimitiveType::Triangle;
}

nlohmann::json Vec3ToJson(const glm::vec3& value) {
    return nlohmann::json::array({ value.x, value.y, value.z });
}

nlohmann::json Vec4ToJson(const glm::vec4& value) {
    return nlohmann::json::array({ value.x, value.y, value.z, value.w });
}

glm::vec3 Vec3FromJson(const nlohmann::json& value) {
    return glm::vec3(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
}

glm::vec4 Vec4FromJson(const nlohmann::json& value) {
    return glm::vec4(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>());
}

} // namespace

bool SceneSerializer::Save(const std::string& path, const std::vector<EntitySnapshot>& snapshots, std::string& errorMessage) {
    try {
        std::filesystem::path targetPath(path);
        std::filesystem::create_directories(targetPath.parent_path());

        nlohmann::json root;
        root["version"] = 1;
        root["entities"] = nlohmann::json::array();

        for (const EntitySnapshot& snapshot : snapshots) {
            root["entities"].push_back({
                { "name", snapshot.Name },
                { "primitive", PrimitiveToString(snapshot.Primitive) },
                { "position", Vec3ToJson(snapshot.Position) },
                { "rotation", Vec3ToJson(snapshot.Rotation) },
                { "scale", Vec3ToJson(snapshot.Scale) },
                { "tint", Vec4ToJson(snapshot.Tint) },
                { "hasGeneratedMaterial", snapshot.HasGeneratedMaterial },
                { "materialPrompt", snapshot.MaterialPrompt }
            });
        }

        std::ofstream output(path);
        output << root.dump(2);
        return true;
    } catch (const std::exception& exception) {
        errorMessage = exception.what();
        return false;
    }
}

bool SceneSerializer::Load(const std::string& path, std::vector<EntitySnapshot>& snapshots, std::string& errorMessage) {
    try {
        std::ifstream input(path);
        if (!input.is_open()) {
            errorMessage = "Could not open scene file: " + path;
            return false;
        }

        nlohmann::json root;
        input >> root;

        snapshots.clear();
        for (const auto& entityJson : root.at("entities")) {
            EntitySnapshot snapshot;
            snapshot.Name                 = entityJson.at("name").get<std::string>();
            snapshot.Primitive            = PrimitiveFromString(entityJson.value("primitive", "triangle"));
            snapshot.Position             = Vec3FromJson(entityJson.at("position"));
            snapshot.Rotation             = Vec3FromJson(entityJson.at("rotation"));
            snapshot.Scale                = Vec3FromJson(entityJson.at("scale"));
            snapshot.Tint                 = Vec4FromJson(entityJson.at("tint"));
            snapshot.HasGeneratedMaterial = entityJson.value("hasGeneratedMaterial", false);
            snapshot.MaterialPrompt       = entityJson.value("materialPrompt", "");
            snapshots.push_back(snapshot);
        }

        return true;
    } catch (const std::exception& exception) {
        errorMessage = exception.what();
        return false;
    }
}

} // namespace Nova
