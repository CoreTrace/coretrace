// SPDX-License-Identifier: Apache-2.0
#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <string_view>
#include <vector>

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
        C = 0,
        CPP = 1,
        Python = 2,
    };

    inline const std::vector<std::string> IPC_TYPES = {
        "standardIO", // default
        "socket",     // deprecated, see ipcDeprecationNotice
        "serve",
    };

    /// Returns the notice to show for a deprecated IPC mode, or an empty string.
    ///
    /// The Unix socket transport predates the tool output sink: only two of the five tools
    /// ever wrote to it, and nothing in the project reads from it. It is kept working for
    /// existing callers and will be removed in a future release.
    [[nodiscard]] inline std::string ipcDeprecationNotice(std::string_view ipc)
    {
        if (ipc != "socket")
        {
            return {};
        }
        return "--ipc=socket is deprecated and will be removed in a future release: only some "
               "tools write to the socket. Use --ipc=standardIO, or --ipc=serve to consume "
               "results over HTTP.";
    }

} // namespace ctrace_defs

#endif // TYPES_HPP
