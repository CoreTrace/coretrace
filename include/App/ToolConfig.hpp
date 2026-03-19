#ifndef APP_TOOL_CONFIG_HPP
#define APP_TOOL_CONFIG_HPP

#include <string>
#include <string_view>

#include "Config/config.hpp"

namespace ctrace
{
    bool applyToolConfigFile(ProgramConfig& config, std::string_view configPath,
                             std::string& errorMessage);
}

#endif // APP_TOOL_CONFIG_HPP
