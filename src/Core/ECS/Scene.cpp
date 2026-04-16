// -----------------------------------------------------------------------------
// Scene.cpp
// -----------------------------------------------------------------------------

#include "Scene.h"

#include <algorithm>
#include <cctype>

namespace Nova {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

} // namespace

entt::entity Scene::CreateEntity(const std::string& name) {
    // Create a blank entity in the registry
    entt::entity entity = m_Registry.create();

    // Every entity gets a name (TagComponent)
    m_Registry.emplace<TagComponent>(entity, name);

    // Every entity gets a transform (position, rotation, scale)
    m_Registry.emplace<TransformComponent>(entity);

    return entity;
}

void Scene::DestroyEntity(entt::entity entity) {
    // destroy() removes the entity and all its attached components
    m_Registry.destroy(entity);
}

entt::entity Scene::FindEntityByName(const std::string& name) const {
    const std::string needle = ToLower(name);
    auto view = m_Registry.view<TagComponent>();
    for (auto entity : view) {
        if (ToLower(view.get<TagComponent>(entity).Name) == needle)
            return entity;
    }

    return entt::null;
}

std::string Scene::GetEntityName(entt::entity entity) const {
    if (!m_Registry.valid(entity) || !m_Registry.all_of<TagComponent>(entity))
        return "";

    return m_Registry.get<TagComponent>(entity).Name;
}

} // namespace Nova
