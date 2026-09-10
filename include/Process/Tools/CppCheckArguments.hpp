// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_TOOLS_CPPCHECK_ARGUMENTS_HPP
#define PROCESS_TOOLS_CPPCHECK_ARGUMENTS_HPP

#include <string>
#include <vector>

namespace ctrace
{
    /**
     * @brief The command line cppcheck is run with.
     *
     * Without an --enable list cppcheck reports errors only, and stays silent
     * about the buffer overruns, dead stores and null dereferences it is
     * perfectly able to see.
     *
     * "information" is deliberately left out: it contributes nothing but a
     * missingIncludeSystem notice per header, which is noise the reader has to
     * wade through, and enough of it to fill a pipe buffer.
     */
    [[nodiscard]] inline std::vector<std::string> cppcheckArguments(bool sarifFormat,
                                                                    const std::string& file)
    {
        std::vector<std::string> arguments;
        if (sarifFormat)
        {
            arguments.emplace_back("--output-format=sarif");
        }
        arguments.emplace_back("--enable=warning,style,performance,portability");
        arguments.push_back(file);
        return arguments;
    }
} // namespace ctrace

#endif // PROCESS_TOOLS_CPPCHECK_ARGUMENTS_HPP
