// SPDX-License-Identifier: Apache-2.0
#ifndef APP_SUPPORTED_TOOLS_HPP
#define APP_SUPPORTED_TOOLS_HPP

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ctrace
{
    inline constexpr std::array<std::string_view, 5> SUPPORTED_TOOLS = {
        "flawfinder", "ikos", "cppcheck", "tscancode", "ctrace_stack_analyzer",
    };

    inline bool isSupportedToolName(std::string_view name)
    {
        for (const auto tool : SUPPORTED_TOOLS)
        {
            if (tool == name)
            {
                return true;
            }
        }
        return false;
    }

    inline std::string supportedToolsCsv()
    {
        std::string joined;
        for (std::size_t i = 0; i < SUPPORTED_TOOLS.size(); ++i)
        {
            if (i > 0)
            {
                joined.push_back(',');
            }
            joined.append(SUPPORTED_TOOLS[i]);
        }
        return joined;
    }

    inline std::vector<std::string>
    normalizeAndValidateToolList(const std::vector<std::string>& tools, std::string& error)
    {
        std::vector<std::string> normalized;
        normalized.reserve(tools.size());

        for (const auto& tool : tools)
        {
            if (!isSupportedToolName(tool))
            {
                error = "Unknown tool '" + tool + "'. Allowed tools: [" + supportedToolsCsv() + "]";
                return {};
            }

            bool alreadyAdded = false;
            for (const auto& current : normalized)
            {
                if (current == tool)
                {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded)
            {
                normalized.push_back(tool);
            }
        }

        error.clear();
        return normalized;
    }
} // namespace ctrace

#endif // APP_SUPPORTED_TOOLS_HPP
