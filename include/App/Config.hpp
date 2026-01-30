#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

namespace ctrace
{
    CT_NODISCARD ProgramConfig buildConfig(int argc, char* argv[]);
}

#endif // APP_CONFIG_HPP
