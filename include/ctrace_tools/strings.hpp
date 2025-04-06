#ifndef STRINGS_HPP
#define STRINGS_HPP

#include <string_view>
#include <vector>
#include <iostream>

namespace ctrace_tools
{

namespace strings
{
    /**
     * @brief Splits a string into parts based on commas.
     *
     * This function takes an input string and splits it into a vector of substrings,
     * using commas (`,`) as the delimiter. The function returns a vector of
     * `std::string_view` objects, which are lightweight views into the original string.
     *
     * @param input The input string to be split, provided as a `std::string_view`.
     * @return A `std::vector<std::string_view>` containing the parts of the string
     *         split by commas. If the input string is empty, the returned vector
     *         will also be empty.
     *
     * @note The function is marked `[[nodiscard]]`, meaning the return value
     *       should not be ignored. It is also `noexcept`, indicating that it
     *       does not throw exceptions.
     */
    [[nodiscard]] std::vector<std::string_view> splitByComma(std::string_view input) noexcept;
}

}

#endif // STRINGS_HPP
