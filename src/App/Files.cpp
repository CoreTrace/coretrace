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
        [[nodiscard]] bool hasPathSegment(const std::filesystem::path& path,
                                          std::string_view segment)
        {
            if (segment.empty())
            {
                return false;
            }
            for (const auto& part : path)
            {
                if (part == std::filesystem::path(segment))
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    CT_NODISCARD std::vector<std::string> resolveSourceFiles(const ProgramConfig& config)
    {
        using json = nlohmann::json;

        std::vector<std::string> sourceFiles;
        std::unordered_set<std::string> seenPaths;
        seenPaths.reserve(config.files.size() + 1);

        auto fileEntries = config.files;
        std::filesystem::path autoDiscoveredCompdbPath;
        bool hasAutoDiscoveredCompdbPath = false;
        if (fileEntries.empty() && !config.global.compile_commands.empty())
        {
            std::filesystem::path compdbPath(config.global.compile_commands);
            std::error_code fsErr;
            if (std::filesystem::is_directory(compdbPath, fsErr))
            {
                compdbPath /= "compile_commands.json";
            }
            if (std::filesystem::is_regular_file(compdbPath, fsErr))
            {
                compdbPath = compdbPath.lexically_normal();
                fileEntries.emplace_back(compdbPath.string());
                autoDiscoveredCompdbPath = compdbPath;
                hasAutoDiscoveredCompdbPath = true;
            }
        }

        sourceFiles.reserve(fileEntries.size());

        const auto appendResolved = [&](const std::string& candidate,
                                        const std::filesystem::path& baseDir) -> bool
        {
            if (candidate.empty())
            {
                return false;
            }
            std::filesystem::path resolved(candidate);
            if (resolved.is_relative() && !baseDir.empty())
            {
                resolved = baseDir / resolved;
            }
            resolved = resolved.lexically_normal();
            const auto resolvedStr = resolved.string();
            if (!seenPaths.insert(resolvedStr).second)
            {
                return false;
            }
            sourceFiles.emplace_back(resolvedStr);
            return true;
        };

        for (const auto& fileConfig : fileEntries)
        {
            const std::string& entry = fileConfig.src_file;
            const std::filesystem::path entryPath(entry);
            const bool isCompdbAutoDiscoveryEntry =
                hasAutoDiscoveredCompdbPath &&
                (entryPath.lexically_normal() == autoDiscoveredCompdbPath);
            const bool filterDependencyEntries =
                isCompdbAutoDiscoveryEntry && !config.global.include_compdb_deps;

            const auto shouldSkipDependencyEntry = [&](const std::string& candidate,
                                                       const std::filesystem::path& baseDir) -> bool
            {
                if (!filterDependencyEntries || candidate.empty())
                {
                    return false;
                }
                std::filesystem::path resolved(candidate);
                if (resolved.is_relative() && !baseDir.empty())
                {
                    resolved = baseDir / resolved;
                }
                return hasPathSegment(resolved.lexically_normal(), "_deps");
            };

            bool expanded = false;
            if (!entry.empty() && (entry.ends_with(".json") || entry.ends_with(".JSON")))
            {
                std::ifstream manifestStream(entry);
                if (manifestStream.is_open())
                {
                    std::ostringstream buffer;
                    buffer << manifestStream.rdbuf();

                    const auto manifest = json::parse(buffer.str(), nullptr, false);
                    if (!manifest.is_discarded())
                    {
                        const std::filesystem::path manifestDir =
                            std::filesystem::path(entry).parent_path();

                        const auto appendFromArray = [&](const json& arr) -> bool
                        {
                            bool appended = false;
                            for (const auto& item : arr)
                            {
                                if (item.is_string())
                                {
                                    const auto candidate = item.get<std::string>();
                                    if (shouldSkipDependencyEntry(candidate, manifestDir))
                                    {
                                        continue;
                                    }
                                    appended |= appendResolved(candidate, manifestDir);
                                }
                                else if (item.is_object())
                                {
                                    if (const auto it = item.find("file");
                                        it != item.end() && it->is_string())
                                    {
                                        std::filesystem::path entryBase = manifestDir;
                                        if (const auto itDir = item.find("directory");
                                            itDir != item.end() && itDir->is_string())
                                        {
                                            entryBase = itDir->get<std::string>();
                                            if (entryBase.is_relative())
                                            {
                                                entryBase = manifestDir / entryBase;
                                            }
                                        }
                                        const auto candidate = it->get<std::string>();
                                        if (shouldSkipDependencyEntry(candidate, entryBase))
                                        {
                                            continue;
                                        }
                                        appended |= appendResolved(candidate, entryBase);
                                    }
                                    else if (const auto itSrc = item.find("src_file");
                                             itSrc != item.end() && itSrc->is_string())
                                    {
                                        const auto candidate = itSrc->get<std::string>();
                                        if (shouldSkipDependencyEntry(candidate, manifestDir))
                                        {
                                            continue;
                                        }
                                        appended |= appendResolved(candidate, manifestDir);
                                    }
                                    else if (const auto itPath = item.find("path");
                                             itPath != item.end() && itPath->is_string())
                                    {
                                        const auto candidate = itPath->get<std::string>();
                                        if (shouldSkipDependencyEntry(candidate, manifestDir))
                                        {
                                            continue;
                                        }
                                        appended |= appendResolved(candidate, manifestDir);
                                    }
                                }
                            }
                            return appended;
                        };

                        if (manifest.is_array())
                        {
                            expanded = appendFromArray(manifest);
                        }
                        else if (manifest.is_object())
                        {
                            if (const auto it = manifest.find("files");
                                it != manifest.end() && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto it = manifest.find("sources");
                                     it != manifest.end() && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto it = manifest.find("compile_commands");
                                     it != manifest.end() && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto itFile = manifest.find("file");
                                     itFile != manifest.end() && itFile->is_string())
                            {
                                expanded = appendResolved(itFile->get<std::string>(), manifestDir);
                            }
                            else if (const auto itSrc = manifest.find("src_file");
                                     itSrc != manifest.end() && itSrc->is_string())
                            {
                                expanded = appendResolved(itSrc->get<std::string>(), manifestDir);
                            }
                            else if (const auto itPath = manifest.find("path");
                                     itPath != manifest.end() && itPath->is_string())
                            {
                                expanded = appendResolved(itPath->get<std::string>(), manifestDir);
                            }
                        }
                    }
                }
            }

            if (!expanded)
            {
                if (!entry.empty() && (entry.ends_with(".json") || entry.ends_with(".JSON")))
                {
                    continue;
                }
                (void)appendResolved(entry, {});
            }
        }

        return sourceFiles;
    }
} // namespace ctrace
