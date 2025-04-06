#ifndef COLORS_HPP
#define COLORS_HPP

#include <string>

namespace ctrace
{
    namespace Color
    {
        /**
         * @brief ANSI escape codes for terminal text colors.
         *
         * The `Color` namespace provides constants for various text colors
         * and a reset code to restore the default terminal color. These can
         * be used to format text output in the terminal with colors.
         */
        const std::string RESET     = "\033[0m";
        const std::string RED       = "\033[31m";
        const std::string GREEN     = "\033[32m";
        const std::string YELLOW    = "\033[33m";
        const std::string BLUE      = "\033[34m";
        const std::string MAGENTA   = "\033[35m";
        const std::string CYAN      = "\033[36m";
    }
}

#endif // COLORS_HPP
