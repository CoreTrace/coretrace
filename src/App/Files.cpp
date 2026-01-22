#include "App/Files.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace ctrace
{
    CT_NODISCARD std::vector<std::string> resolveSourceFiles(const ProgramConfig& config)
    {
        using json = nlohmann::json;

        std::vector<std::string> sourceFiles;
        sourceFiles.reserve(config.files.size());

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
            sourceFiles.emplace_back(resolved.string());
            return true;
        };

        for (const auto& fileConfig : config.files)
        {
            const std::string& entry = fileConfig.src_file;

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
                        const std::filesystem::path manifestDir = std::filesystem::path(entry).parent_path();

                        const auto appendFromArray = [&](const json& arr) -> bool
                        {
                            bool appended = false;
                            for (const auto& item : arr)
                            {
                                if (item.is_string())
                                {
                                    appended |= appendResolved(item.get<std::string>(), manifestDir);
                                }
                                else if (item.is_object())
                                {
                                    if (const auto it = item.find("file"); it != item.end() && it->is_string())
                                    {
                                        appended |= appendResolved(it->get<std::string>(), manifestDir);
                                    }
                                    else if (const auto itSrc = item.find("src_file"); itSrc != item.end()
                                          && itSrc->is_string())
                                    {
                                        appended |= appendResolved(itSrc->get<std::string>(), manifestDir);
                                    }
                                    else if (const auto itPath = item.find("path"); itPath != item.end()
                                          && itPath->is_string())
                                    {
                                        appended |= appendResolved(itPath->get<std::string>(), manifestDir);
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
                            if (const auto it = manifest.find("files"); it != manifest.end() && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto it = manifest.find("sources"); it != manifest.end() && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto it = manifest.find("compile_commands"); it != manifest.end()
                                  && it->is_array())
                            {
                                expanded = appendFromArray(*it);
                            }
                            else if (const auto itFile = manifest.find("file"); itFile != manifest.end()
                                  && itFile->is_string())
                            {
                                expanded = appendResolved(itFile->get<std::string>(), manifestDir);
                            }
                            else if (const auto itSrc = manifest.find("src_file"); itSrc != manifest.end()
                                  && itSrc->is_string())
                            {
                                expanded = appendResolved(itSrc->get<std::string>(), manifestDir);
                            }
                            else if (const auto itPath = manifest.find("path"); itPath != manifest.end()
                                  && itPath->is_string())
                            {
                                expanded = appendResolved(itPath->get<std::string>(), manifestDir);
                            }
                        }
                    }
                }
            }

            if (!expanded)
            {
                sourceFiles.emplace_back(entry);
            }
        }

        return sourceFiles;
    }
}
