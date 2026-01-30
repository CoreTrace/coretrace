#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <memory>
#include <getopt.h> // Nécessaire pour getopt
#include "ArgumentParser/BaseArgumentParser.hpp"

// Implémentation avec getopt
class GetoptArgumentParser : public BaseArgumentParser
{
  private:
    std::vector<struct option> _longOptions; // Pour getopt_long
    std::vector<OptionInfo> _options;        // Liste des options enregistrées
    std::string _optString;                  // Chaîne pour getopt (ex: "vha:")

    // Convertit le nom long en format attendu par getopt_long (sans "--")
    static std::string stripDashes(const std::string& name)
    {
        std::string stripped = name;
        while (!stripped.empty() && stripped[0] == '-')
        {
            stripped.erase(0, 1);
        }
        return stripped;
    }

  public:
    void addOption(const std::string& name, bool hasArgument, char shortName) override
    {
        std::cout << "GetOpt is called" << std::endl;
        // Ajouter l'option à la liste interne
        OptionInfo opt;
        opt.name = name;
        opt.shortName = shortName;
        opt.hasArgument = hasArgument;
        opt.isSet = false;
        _options.push_back(opt);

        // Ajouter à la chaîne d'options pour getopt
        if (shortName != '\0')
        {
            _optString += shortName;
            if (hasArgument)
            {
                _optString += ':';
            }
        }

        // Ajouter à la liste des options longues pour getopt_long
        struct option longOpt = {0};
        longOpt.name = stripDashes(name).c_str();
        longOpt.has_arg = hasArgument ? required_argument : no_argument;
        longOpt.flag = nullptr;
        longOpt.val = shortName != '\0' ? shortName : 0;
        _longOptions.push_back(longOpt);
    }

    void addFlag(const std::string& name, char shortName) override
    {
        // Un flag est une option sans argument
        addOption(name, false, shortName);
    }

    bool parse(int argc, char* argv[]) override
    {
        // Réinitialiser les erreurs
        setError(ErrorCode::SUCCESS);

        // Ajouter une entrée finale vide pour getopt_long
        struct option endOption = {0, 0, 0, 0};
        _longOptions.push_back(endOption);

        // Reset getopt
        optind = 1;

        int longindex = 0;
        int c;
        while ((c = getopt_long(argc, argv, _optString.c_str(), _longOptions.data(), &longindex)) !=
               -1)
        {
            if (c == '?')
            {
                setError(ErrorCode::INVALID_OPTION, "Unknown option");
                return false;
            }
            if (c == ':')
            {
                setError(ErrorCode::MISSING_ARGUMENT, "Missing argument");
                return false;
            }

            // Trouver l'option correspondante
            for (auto& opt : _options)
            {
                if (c == opt.shortName ||
                    (c != 0 && stripDashes(opt.name) == _longOptions[longindex].name))
                {
                    opt.isSet = true;

                    // Vérifier si un argument est attendu
                    if (opt.hasArgument)
                    {
                        if (optarg)
                        {
                            opt.value = optarg; // Format `--option=value`
                        }
                        else if (optind < argc && argv[optind][0] != '-')
                        {
                            opt.value = argv[optind]; // Format `--option value`
                            optind++;                 // Avancer l'index
                        }
                        else
                        {
                            setError(ErrorCode::MISSING_ARGUMENT,
                                     "Missing argument for option: " + opt.name);
                            return false;
                        }
                    }
                    break;
                }
            }
        }

        return true;
    }

    bool hasOption(const std::string& name) const override
    {
        for (const auto& opt : _options)
        {
            std::cout << opt.name << " -> " << opt.shortName << std::endl;
            if (opt.name == name ||
                (opt.shortName != '\0' && std::string(1, opt.shortName) == name))
            {
                return opt.isSet;
            }
        }
        return false;
    }

    std::string getOptionValue(const std::string& name) const override
    {
        for (const auto& opt : _options)
        {
            if (opt.name == name ||
                (opt.shortName != '\0' && std::string(1, opt.shortName) == name))
            {
                return opt.value;
            }
        }
        return "";
    }
};

// Votre classe originale modifiée pour utiliser l'interface
// class ArgumentManager {
// public:
//     explicit ArgumentManager(std::unique_ptr<IArgumentParser> parser)
//         : _parser(std::move(parser)) {}

//     void addOption(const std::string& name, bool hasArgument, char shortName) {
//         _parser->addOption(name, hasArgument, shortName);
//     }

//     void addFlag(const std::string& name, char shortName) {
//         _parser->addFlag(name, shortName);
//     }

//     void parse(int argc, char* argv[]) {
//         if (!_parser->parse(argc, argv)) {
//             std::cerr << "Error: " << _parser->getErrorMessage() << std::endl;
//             std::exit(static_cast<int>(_parser->getLastError()));
//         }
//     }

//     // Ajout de la méthode pour accéder au parser
//     IArgumentParser* getParser() const {
//         return _parser.get();
//     }

// private:
//     std::unique_ptr<IArgumentParser> _parser;
// };

// Exemple d'utilisation
// int main(int argc, char* argv[]) {
//     // Création avec getopt
//     auto parser = std::make_unique<GetoptArgumentParser>();
//     ArgumentManager argManager(std::move(parser));

//     // Configuration
//     argManager.addOption("verbose", false, 'v');
//     argManager.addOption("output", true, 'o');
//     argManager.addFlag("help", 'h');

//     // Parsing
//     argManager.parse(argc, argv);

//     // Exemple d'utilisation des résultats
//     if (argManager.getParser()->hasOption("--verbose")) {
//         std::cout << "Verbose mode enabled" << std::endl;
//     }
//     if (argManager.getParser()->hasOption("--help")) {
//         std::cout << "Help requested" << std::endl;
//     }
//     if (argManager.getParser()->hasOption("--output")) {
//         std::cout << "Output file: " << argManager.getParser()->getOptionValue("--output") << std::endl;
//     }

//     return 0;
// }
