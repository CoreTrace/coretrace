#include "ctrace_tools/languageType.hpp"

namespace ctrace_tools {
    // Fonction pour détecter le type de langage à partir d'une extension de fichier
    [[nodiscard]] ctrace_defs::LanguageType detectLanguage(std::string_view filename) noexcept {
        // Liste des extensions typiques
        constexpr std::string_view cExtensions[] = {".c", ".h"};
        constexpr std::string_view cppExtensions[] = {".cpp", ".hpp", ".cxx", ".cc", ".hxx"};

        // Recherche dans les extensions C
        for (const auto& ext : cExtensions) {
            if (filename.ends_with(ext)) {
                return ctrace_defs::LanguageType::C;
            }
        }

        // Recherche dans les extensions C++
        for (const auto& ext : cppExtensions) {
            if (filename.ends_with(ext)) {
                return ctrace_defs::LanguageType::CPP;
            }
        }

        // Par défaut, on suppose C++ (choix arbitraire, ajustable)
        return ctrace_defs::LanguageType::CPP;
    }
}
