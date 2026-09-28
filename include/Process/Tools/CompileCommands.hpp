// SPDX-License-Identifier: Apache-2.0
#ifndef COMPILE_COMMANDS_HPP
#define COMPILE_COMMANDS_HPP

#include "Config/config.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ctrace
{
    /// One entry of a Clang compilation database, reduced to what replays its compilation
    /// elsewhere: the source, resolved against the entry's directory, and the arguments that say
    /// how the source is interpreted (include paths, macros, standard, target). The compiler's
    /// name, the source itself, output and dependency-file selection and optimization levels
    /// are left out: the consumer chooses its own output and compiles unoptimized, as an
    /// optimizer removes the very structure an analysis reads.
    struct CompileCommand
    {
        std::filesystem::path file;
        std::vector<std::string> arguments;
    };

    /// Reads `compile_commands.json`: entries with an "arguments" array or, as CMake writes
    /// them, a "command" string, split as a POSIX shell would (quotes, backslash escapes). A
    /// source listed several times, once per target it is built into, keeps its first entry.
    /// Nothing, and `error` set, when the file cannot be read or is not such a database.
    [[nodiscard]] std::optional<std::vector<CompileCommand>>
    readCompileCommands(const std::filesystem::path& database, std::string& error);

    /// The database `config` names: `files.compile_commands`, a file or the directory holding
    /// one, else the first `.json` input (resolveSourceFiles reads sources from both). Empty
    /// when the run has none.
    [[nodiscard]] std::filesystem::path compileCommandsPath(const ProgramConfig& config);
} // namespace ctrace

#endif // COMPILE_COMMANDS_HPP
