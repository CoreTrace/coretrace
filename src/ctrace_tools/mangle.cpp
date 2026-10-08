// SPDX-License-Identifier: Apache-2.0
#include "ctrace_tools/mangle.hpp"

#include <cstdlib>
#include <memory>
#include <sstream>

#include <cxxabi.h>

namespace ctrace_tools::mangle
{

    bool isMangled(std::string_view name) noexcept
    {
        int status = 0;

        if (!name.starts_with("_Z"))
        {
            return false;
        }

        std::unique_ptr<char, void (*)(void*)> demangled(
            abi::__cxa_demangle(std::string(name).c_str(), nullptr, nullptr, &status), std::free);
        return status == 0;
    }

    std::string mangleFunction(const std::string& namespaceName, const std::string& functionName,
                               const std::vector<std::string>& paramTypes)
    {
        std::stringstream mangled;

        // Standard prefix of C++ symbols in the Itanium ABI
        mangled << "_Z";

        // A namespace opens a nested name with 'N' followed by the encoded namespace
        if (!namespaceName.empty())
        {
            mangled << "N";
            mangled << namespaceName.length() << namespaceName;
        }

        // The function name, prefixed with its length
        mangled << functionName.length() << functionName;

        // The nested name ends with 'E', before the parameter types
        if (!namespaceName.empty())
        {
            mangled << "E";
        }

        // The parameter types
        for (const std::string& param : paramTypes)
        {
            if (param == "int")
            {
                mangled << "i";
            }
            else if (param == "double")
            {
                mangled << "d";
            }
            else if (param == "char")
            {
                mangled << "c";
            }
            else if (param == "std::string")
            {
                mangled << "Ss"; // 'S' for substitution, 's' for std::string
            }
            else if (param == "float")
            {
                mangled << "f";
            }
            else if (param == "bool")
            {
                mangled << "b";
            }
            else if (param == "void")
            {
                mangled << "v";
            }
            else
            {
                // Other types are encoded as their length followed by their name
                mangled << param.length() << param;
            }
        }

        return mangled.str();
    }

} // namespace ctrace_tools::mangle
