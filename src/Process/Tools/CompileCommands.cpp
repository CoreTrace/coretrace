// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/CompileCommands.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

namespace ctrace
{
    namespace
    {
        /// Splits a command line as a POSIX shell would: blanks separate words; single quotes
        /// keep everything up to the next one; double quotes keep everything but a backslash
        /// before a quote or a backslash; a backslash outside quotes keeps the next character.
        [[nodiscard]] std::vector<std::string> splitCommandLine(std::string_view command)
        {
            std::vector<std::string> words;
            std::string word;
            bool inWord = false;
            for (std::size_t i = 0; i < command.size(); ++i)
            {
                const char ch = command[i];
                if (ch == '\'')
                {
                    inWord = true;
                    for (++i; i < command.size() && command[i] != '\''; ++i)
                    {
                        word += command[i];
                    }
                    continue;
                }
                if (ch == '"')
                {
                    inWord = true;
                    for (++i; i < command.size() && command[i] != '"'; ++i)
                    {
                        if (command[i] == '\\' && i + 1 < command.size() &&
                            (command[i + 1] == '"' || command[i + 1] == '\\'))
                        {
                            ++i;
                        }
                        word += command[i];
                    }
                    continue;
                }
                if (ch == '\\' && i + 1 < command.size())
                {
                    inWord = true;
                    word += command[++i];
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(ch)) != 0)
                {
                    if (inWord)
                    {
                        words.push_back(std::move(word));
                        word.clear();
                        inWord = false;
                    }
                    continue;
                }
                inWord = true;
                word += ch;
            }
            if (inWord)
            {
                words.push_back(std::move(word));
            }
            return words;
        }

        /// Options the consumer supplies itself, or that would redirect the compiler's output.
        [[nodiscard]] bool isOutputOrDependencyOption(std::string_view argument)
        {
            static constexpr std::array dropped = {
                std::string_view{"-o"},   std::string_view{"-c"},  std::string_view{"-MD"},
                std::string_view{"-MMD"}, std::string_view{"-MF"}, std::string_view{"-MT"},
                std::string_view{"-MQ"},  std::string_view{"-MP"},
            };
            return std::find(dropped.begin(), dropped.end(), argument) != dropped.end();
        }

        [[nodiscard]] bool takesSeparateValue(std::string_view argument)
        {
            static constexpr std::array withValue = {
                std::string_view{"-o"},
                std::string_view{"-MF"},
                std::string_view{"-MT"},
                std::string_view{"-MQ"},
            };
            return std::find(withValue.begin(), withValue.end(), argument) != withValue.end();
        }

        /// `-O`, `-O0`..`-O3`, `-Os`, `-Oz`, `-Og`, `-Ofast` and `-flto`. Anything longer that
        /// starts the same way is another option, and is kept.
        [[nodiscard]] bool isOptimizationOption(std::string_view argument)
        {
            if (argument == "-flto" || argument.starts_with("-flto="))
            {
                return true;
            }
            if (!argument.starts_with("-O"))
            {
                return false;
            }
            const std::string_view level = argument.substr(2);
            return level.empty() || level == "fast" ||
                   (level.size() == 1 && (std::isdigit(static_cast<unsigned char>(level[0])) != 0 ||
                                          level[0] == 's' || level[0] == 'z' || level[0] == 'g'));
        }

        [[nodiscard]] std::filesystem::path resolveAgainst(const std::filesystem::path& directory,
                                                           std::filesystem::path path)
        {
            if (path.is_relative())
            {
                path = directory / path;
            }
            return path.lexically_normal();
        }

        /// Drops the compiler, the source, output selection and optimization from `raw`.
        [[nodiscard]] std::vector<std::string>
        replayableArguments(const std::vector<std::string>& raw, const std::filesystem::path& file,
                            const std::filesystem::path& directory)
        {
            std::vector<std::string> kept;
            for (std::size_t index = 1; index < raw.size(); ++index)
            {
                const std::string& argument = raw[index];
                if (isOutputOrDependencyOption(argument))
                {
                    if (takesSeparateValue(argument))
                    {
                        ++index;
                    }
                    continue;
                }
                if ((argument.starts_with("-o") && argument.size() > 2) ||
                    isOptimizationOption(argument) || resolveAgainst(directory, argument) == file)
                {
                    continue;
                }
                kept.push_back(argument);
            }
            return kept;
        }

        [[nodiscard]] std::optional<std::vector<std::string>>
        rawArgumentsOf(const nlohmann::json& entry)
        {
            if (const auto arguments = entry.find("arguments");
                arguments != entry.end() && arguments->is_array())
            {
                std::vector<std::string> parsed;
                for (const nlohmann::json& argument : *arguments)
                {
                    if (!argument.is_string())
                    {
                        return std::nullopt;
                    }
                    parsed.push_back(argument.get<std::string>());
                }
                return parsed;
            }
            if (const auto command = entry.find("command");
                command != entry.end() && command->is_string())
            {
                return splitCommandLine(command->get<std::string>());
            }
            return std::nullopt;
        }
    } // namespace

    std::optional<std::vector<CompileCommand>>
    readCompileCommands(const std::filesystem::path& database, std::string& error)
    {
        std::ifstream stream(database);
        if (!stream.is_open())
        {
            error = "Unable to open compile database '" + database.string() + "'.";
            return std::nullopt;
        }
        const nlohmann::json document = nlohmann::json::parse(stream, nullptr, false);
        const std::string notADatabase =
            "'" + database.string() +
            "' is not a compile_commands.json document: expected an array of objects with a "
            "\"file\" and \"arguments\" or a \"command\".";
        if (document.is_discarded() || !document.is_array())
        {
            error = notADatabase;
            return std::nullopt;
        }

        const std::filesystem::path databaseDir = database.parent_path();
        std::vector<CompileCommand> commands;
        std::set<std::filesystem::path> seen;
        for (const nlohmann::json& entry : document)
        {
            const auto file = entry.is_object() ? entry.find("file") : entry.end();
            if (!entry.is_object() || file == entry.end() || !file->is_string())
            {
                error = notADatabase;
                return std::nullopt;
            }
            std::filesystem::path directory = databaseDir;
            if (const auto dir = entry.find("directory"); dir != entry.end() && dir->is_string())
            {
                directory = resolveAgainst(databaseDir, dir->get<std::string>());
            }
            const std::filesystem::path source =
                resolveAgainst(directory, file->get<std::string>());
            const auto raw = rawArgumentsOf(entry);
            if (!raw)
            {
                error = notADatabase;
                return std::nullopt;
            }
            if (seen.insert(source).second)
            {
                commands.push_back({source, replayableArguments(*raw, source, directory)});
            }
        }
        error.clear();
        return commands;
    }

    std::filesystem::path compileCommandsPath(const ProgramConfig& config)
    {
        if (!config.files.compile_commands.empty())
        {
            std::filesystem::path candidate(config.files.compile_commands);
            std::error_code err;
            if (std::filesystem::is_directory(candidate, err))
            {
                candidate /= "compile_commands.json";
            }
            return candidate;
        }
        for (const std::string& input : config.files.input)
        {
            if (input.ends_with(".json") || input.ends_with(".JSON"))
            {
                return input;
            }
        }
        return {};
    }
} // namespace ctrace
