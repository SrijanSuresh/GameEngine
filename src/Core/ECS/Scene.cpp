// -----------------------------------------------------------------------------
// Scene.cpp
// -----------------------------------------------------------------------------

#include "Scene.h"

namespace Nova {

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

} // namespace Nova