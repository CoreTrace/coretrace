// SPDX-License-Identifier: Apache-2.0
#include "App/Files.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace ctrace
{
    namespace
    {
        using json = nlohmann::json;

        [[nodiscard]] bool endsWithJson(std::string_view path)
        {
            return path.ends_with(".json") || path.ends_with(".JSON");
        }

        [[nodiscard]] bool hasPathSegment(const std::filesystem::path& path,
                                          std::string_view segment)
        {
            for (const auto& part : path)
            {
                if (part == std::filesystem::path(segment))
                {
                    return true;
                }
            }
            return false;
        }

        /// Keeps insertion order and drops duplicates after normalization.
        class FileList
        {
          public:
            void add(const std::filesystem::path& path)
            {
                const std::string normalized = path.lexically_normal().string();
                if (seen_.insert(normalized).second)
                {
                    files_.push_back(normalized);
                }
            }

            std::vector<std::string> take()
            {
                return std::move(files_);
            }

          private:
            std::unordered_set<std::string> seen_;
            std::vector<std::string> files_;
        };

        /// Reads a Clang compilation database and appends its source files.
        [[nodiscard]] bool appendCompileDatabase(const std::filesystem::path& databasePath,
                                                 bool skipDependencyEntries, FileList& out,
                                                 std::string& error)
        {
            std::ifstream stream(databasePath);
            if (!stream.is_open())
            {
                error = "Unable to open compile database '" + databasePath.string() + "'.";
                return false;
            }
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            const json document = json::parse(buffer.str(), nullptr, false);
            if (document.is_discarded())
            {
                error = "Invalid JSON in compile database '" + databasePath.string() + "'.";
                return false;
            }

            const std::string notADatabase =
                "'" + databasePath.string() +
                "' is not a compile_commands.json document: expected an array of objects "
                "with a \"file\" field.";
            if (!document.is_array())
            {
                error = notADatabase;
                return false;
            }

            const std::filesystem::path databaseDir = databasePath.parent_path();
            for (const json& entry : document)
            {
                const auto file = entry.is_object() ? entry.find("file") : entry.end();
                if (!entry.is_object() || file == entry.end() || !file->is_string())
                {
                    error = notADatabase;
                    return false;
                }

                std::filesystem::path base = databaseDir;
                if (const auto directory = entry.find("directory");
                    directory != entry.end() && directory->is_string())
                {
                    base = directory->get<std::string>();
                    if (base.is_relative())
                    {
                        base = databaseDir / base;
                    }
                }

                std::filesystem::path source = file->get<std::string>();
                if (source.empty())
                {
                    continue;
                }
                if (source.is_relative())
                {
                    source = base / source;
                }
                if (skipDependencyEntries && hasPathSegment(source.lexically_normal(), "_deps"))
                {
                    continue;
                }
                out.add(source);
            }
            return true;
        }
    } // namespace

    CT_NODISCARD SourceFileResolution resolveSourceFiles(const ProgramConfig& config)
    {
        SourceFileResolution result;
        FileList files;

        std::vector<std::string> inputs = config.files.input;
        std::filesystem::path autoDiscovered;
        if (inputs.empty() && !config.files.compile_commands.empty())
        {
            std::filesystem::path candidate(config.files.compile_commands);
            std::error_code fsError;
            if (std::filesystem::is_directory(candidate, fsError))
            {
                candidate /= "compile_commands.json";
            }
            if (std::filesystem::is_regular_file(candidate, fsError))
            {
                autoDiscovered = candidate.lexically_normal();
                inputs.push_back(autoDiscovered.string());
            }
        }

        for (const std::string& input : inputs)
        {
            if (input.empty())
            {
                continue;
            }
            if (!endsWithJson(input))
            {
                files.add(input);
                continue;
            }

            const std::filesystem::path databasePath(input);
            const bool discovered =
                !autoDiscovered.empty() && databasePath.lexically_normal() == autoDiscovered;
            const bool skipDependencyEntries = discovered && !config.files.include_compdb_deps;
            if (!appendCompileDatabase(databasePath, skipDependencyEntries, files, result.error))
            {
                return result;
            }
        }

        result.files = files.take();
        return result;
    }
} // namespace ctrace
