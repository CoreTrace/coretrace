#ifndef LANGUAGE_TYPE_HPP
#define LANGUAGE_TYPE_HPP

#include <string_view>
#include "ctrace_defs/types.hpp"

namespace ctrace_tools
{
    /**
     * @brief Detects the programming language type based on a file extension.
     *
     * This function takes a filename as input and determines the programming
     * language type by analyzing its extension. It returns a value of type
     * `ctrace_defs::LanguageType`, which represents the detected language.
     *
     * @param filename The name of the file (as a `std::string_view`) whose extension
     *                 will be used to detect the language type.
     * @return A `ctrace_defs::LanguageType` value representing the detected language.
     *         If the language cannot be determined, a default or unknown value
     *         may be returned.
     *
     * @note The function is marked `[[nodiscard]]`, which means the return value
     *       should not be ignored. It is also `noexcept`, indicating that it does
     *       not throw exceptions.
     */
    [[nodiscard]] ctrace_defs::LanguageType detectLanguage(std::string_view filename) noexcept;
}

#endif // LANGUAGE_TYPE_HPP
