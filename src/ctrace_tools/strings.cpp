// SPDX-License-Identifier: Apache-2.0
#include "ctrace_tools/strings.hpp"

namespace ctrace_tools
{

    namespace strings
    {
        [[nodiscard]] std::vector<std::string_view> splitByComma(std::string_view input) noexcept
        {
            std::vector<std::string_view> result;
            size_t pos = 0;
            bool in_quotes = false;
            size_t start = 0;

            while (pos < input.size())
            {
                if (input[pos] == '"')
                {
                    in_quotes = !in_quotes;
                }
                else if (input[pos] == ',' && !in_quotes)
                {
                    std::string_view token = input.substr(start, pos - start);
                    if (!token.empty() && token.front() == '"' && token.back() == '"')
                    {
                        token.remove_prefix(1);
                        token.remove_suffix(1);
                    }
                    token.remove_prefix(std::min(token.find_first_not_of(" "), token.size()));
                    token.remove_suffix(
                        std::min(token.size() - token.find_last_not_of(" ") - 1, token.size()));
                    if (!token.empty())
                    {
                        result.push_back(token);
                    }
                    start = pos + 1;
                }
                ++pos;
            }

            if (start < input.size())
            {
                std::string_view token = input.substr(start);
                if (!token.empty() && token.front() == '"' && token.back() == '"')
                {
                    token.remove_prefix(1);
                    token.remove_suffix(1);
                }
                token.remove_prefix(std::min(token.find_first_not_of(" "), token.size()));
                token.remove_suffix(
                    std::min(token.size() - token.find_last_not_of(" ") - 1, token.size()));
                if (!token.empty())
                {
                    result.push_back(token);
                }
            }

            return result;
        }

        [[nodiscard]] std::string joinByComma(const std::vector<std::string>& items)
        {
            std::string joined;
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                if (i > 0)
                {
                    joined.push_back(',');
                }
                joined += items[i];
            }
            return joined;
        }
    } // namespace strings

} // namespace ctrace_tools
