#include "tool_paths.hpp"

#include <cstdlib>
#include <string>

ToolPath tool_path_from_env(const char* env_var, const char* compiled_default) {
    const char* value = std::getenv(env_var);
    // An empty override reads as "not set" rather than as an empty path: the
    // shells and container runtimes that carry these variables around export
    // an unset one as empty, so treating it as an answer would break exactly
    // the deployments the override exists for.
    if (value != nullptr && *value != '\0') {
        return ToolPath{std::string(value), true};
    }
    return ToolPath{std::string(compiled_default), false};
}
