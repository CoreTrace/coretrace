// SPDX-License-Identifier: Apache-2.0
#include "ctrace_tools/languageType.hpp"

namespace ctrace_tools
{
    // Detects the language of a file from its extension
    [[nodiscard]] ctrace_defs::LanguageType detectLanguage(std::string_view filename) noexcept
    {
        if (filename.ends_with(".py"))
        {
            return ctrace_defs::LanguageType::Python;
        }

        // Usual extensions of each language
        constexpr std::string_view cExtensions[] = {".c", ".h"};
        constexpr std::string_view cppExtensions[] = {".cpp", ".hpp", ".cxx", ".cc", ".hxx"};

        // C extensions
        for (const auto& ext : cExtensions)
        {
            if (filename.ends_with(ext))
            {
                return ctrace_defs::LanguageType::C;
            }
        }

        // C++ extensions
        for (const auto& ext : cppExtensions)
        {
            if (filename.ends_with(ext))
            {
                return ctrace_defs::LanguageType::CPP;
            }
        }

        // Any other file is assumed to be C++
        return ctrace_defs::LanguageType::CPP;
    }
} // namespace ctrace_tools
