#pragma once

#include "Core/Commands/SceneCommand.h"

#include <string>
#include <vector>

namespace Nova {

class SceneSerializer {
public:
    static bool Save(const std::string& path, const std::vector<EntitySnapshot>& snapshots, std::string& errorMessage);
    static bool Load(const std::string& path, std::vector<EntitySnapshot>& snapshots, std::string& errorMessage);
};

} // namespace Nova
