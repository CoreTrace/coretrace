#ifndef APP_FILES_HPP
#define APP_FILES_HPP

#include <string>
#include <vector>

#include "Config/config.hpp"
#include "attributes.hpp"

namespace ctrace
{
    CT_NODISCARD std::vector<std::string> resolveSourceFiles(const ProgramConfig& config);
}

#endif // APP_FILES_HPP
