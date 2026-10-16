// SPDX-License-Identifier: Apache-2.0
#ifndef APP_FILES_HPP
#define APP_FILES_HPP

#include <string>
#include <vector>

#include "Config/config.hpp"
#include "attributes.hpp"

namespace ctrace
{
    struct SourceFileResolution
    {
        /// Normalized, deduplicated, in input order, each passed to the tools as is. A relative
        /// path that starts with `-` keeps `./` in front of it (notAnOption): normalizing it
        /// again would drop it, and the tools would read the file as an option.
        std::vector<std::string> files;
        std::string error; ///< Non-empty when an input could not be used.

        [[nodiscard]] bool ok() const noexcept
        {
            return error.empty();
        }
    };

    /// Turns `files.input` into the list of source files to analyse.
    ///
    /// Two input forms are accepted: a plain source path, and a `compile_commands.json`
    /// (array of objects with a "file" and optional "directory", the Clang schema), whose
    /// entries resolve against their directory. When `files.input` is empty and
    /// `files.compile_commands` names a database or its directory, that database is used and
    /// its `_deps` entries are skipped unless `files.include_compdb_deps` is set.
    /// Any other `.json` input is an error.
    CT_NODISCARD SourceFileResolution resolveSourceFiles(const ProgramConfig& config);
} // namespace ctrace

#endif // APP_FILES_HPP
