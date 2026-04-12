#pragma once

// -----------------------------------------------------------------------------
// Scene.h
//
// A Scene owns all the entities and their components for one game world.
// It wraps EnTT's registry to give us a cleaner, more readable API.
//
// Usage:
//   Scene scene;
//   entt::entity e = scene.CreateEntity("My Object");
//   scene.GetRegistry().emplace<TransformComponent>(e);
// -----------------------------------------------------------------------------

#include "Components.h"

#include <entt/entt.hpp>

#include <string>

namespace Nova {

class Scene {
public:
    Scene()  = default;
    ~Scene() = default;

    // Creates a new entity with a TagComponent (name) and a TransformComponent.
    // Every entity gets these two by default — same pattern as Unity/Godot.
    entt::entity CreateEntity(const std::string& name);

    // Removes an entity and all its components from the registry.
    void DestroyEntity(entt::entity entity);

    // Returns the raw EnTT registry.
    // Systems and the editor use this to iterate and modify components.
    entt::registry& GetRegistry() { return m_Registry; }

    // Returns true if the entity handle is still valid (not destroyed).
    bool IsValid(entt::entity entity) const { return m_Registry.valid(entity); }

private:
    entt::registry m_Registry;
};

} // namespace Nova