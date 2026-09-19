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
        std::vector<std::string> files; ///< Normalized, deduplicated, in input order.
        std::string error;              ///< Non-empty when an input could not be used.

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
