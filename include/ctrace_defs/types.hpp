#ifndef TYPES_HPP
#define TYPES_HPP

#include <vector>
#include <string>

namespace ctrace_defs
{
    /**
     * @brief Enumeration representing different programming language types.
     *
     * The `LanguageType` enum class defines constants for supported programming
     * languages. Each constant is associated with an integer value.
     */
    enum class LanguageType : int
    {
        C   = 0,
        CPP = 1,
    };

    inline const std::vector<std::string> IPC_TYPES = {
                                                        "standardIO", // default
                                                        "socket",
                                                        // "pipe" // future implementation
                                                    };

}

#endif // TYPES_HPP
