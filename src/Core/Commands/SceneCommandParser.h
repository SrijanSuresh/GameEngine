#pragma once

#include "SceneCommand.h"

#include <string>

namespace Nova {

class SceneCommandParser {
public:
    static ParsedCommandResult Parse(const std::string& input, const std::string& selectedEntityName);
};

} // namespace Nova
