// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <concepts>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <llvm/Demangle/Demangle.h>

namespace ctrace_tools::mangle
{
    /**
     * @brief Concept to define types that can be converted to `std::string_view`.
     *
     * The `StringLike` concept ensures that any type passed to functions
     * requiring it can be implicitly converted to `std::string_view`.
     */
    template <typename T> concept StringLike = std::convertible_to<T, std::string_view>;
    /**
     * @brief Checks if a given name is a mangled C++ symbol.
     *
     * This function uses LLVM's cross-platform demangler so it can recognize
     * both Itanium-style names (`_Z...`) and Microsoft-style names (`?...`).
     *
     * @tparam T A type satisfying the `StringLike` concept.
     * @param name The name to check for mangling.
     * @return `true` if the name is mangled, `false` otherwise.
     */
    [[nodiscard]] bool isMangled(StringLike auto name) noexcept
    {
        const std::string_view sv{name};

        const bool looksMangled =
            sv.starts_with("_Z") || sv.starts_with("?") || sv.starts_with(".?");
        if (!looksMangled)
        {
            return false;
        }

        return llvm::demangle(sv) != sv;
    }

    /**
     * @brief Generates a mangled name for a function.
     *
     * This function creates a mangled name for a function based on its namespace,
     * name, and parameter types. The mangling follows the Itanium C++ ABI conventions.
     *
     * @param namespaceName The namespace of the function.
     * @param functionName The name of the function.
     * @param paramTypes A vector of strings representing the parameter types.
     * @return A `std::string` containing the mangled name.
     *
     * @note The implementation of this function is not provided in the current file.
     */
    [[nodiscard]] std::string mangleFunction(const std::string& namespaceName,
                                             const std::string& functionName,
                                             const std::vector<std::string>& paramTypes);
}; // namespace ctrace_tools::mangle
