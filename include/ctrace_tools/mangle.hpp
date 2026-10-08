// SPDX-License-Identifier: Apache-2.0
#ifndef MANGLE_HPP
#define MANGLE_HPP

#include <string>
#include <string_view>
#include <vector>

namespace ctrace_tools::mangle
{

    // TODO: add mangling for windows

    /*
     * @brief Checks if a given name is a mangled C++ symbol.
     *
     * This function determines whether a given name follows the Itanium C++ ABI
     * mangling conventions (e.g., names starting with `_Z`).
     *
     * @param name The name to check for mangling.
     * @return `true` if the name is mangled, `false` otherwise.
     *
     * @note This function uses `abi::__cxa_demangle` to attempt demangling.
     *       If the demangling succeeds, the name is considered mangled.
     * @note This implementation is specific to platforms using the Itanium C++ ABI
     *       (e.g., Linux, macOS). Windows mangling is not yet supported.
     * @note The function is marked `[[nodiscard]]`, meaning the return value
     *       should not be ignored. It is also `noexcept`, indicating that it
     *       does not throw exceptions.
     * @note A memory allocation failure will end the program because of 'noexcept'.
     */
    [[nodiscard]] bool isMangled(std::string_view name) noexcept;

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
} // namespace ctrace_tools::mangle

#endif // MANGLE_HPP
