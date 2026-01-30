#ifndef GETOPT_ARGUMENT_PARSER_HPP
#define GETOPT_ARGUMENT_PARSER_HPP

#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <memory>
#include <getopt.h>
#include "../BaseArgumentParser.hpp"

/**
 * @brief Argument parser implementation using `getopt` and `getopt_long`.
 *
 * The `GetoptArgumentParser` class provides an implementation of the `IArgumentParser`
 * interface using the `getopt` and `getopt_long` functions for parsing command-line
 * arguments. It supports both short and long options, as well as options with or
 * without arguments.
 */
class GetoptArgumentParser : public BaseArgumentParser
{
  private:
    std::vector<struct option> _longOptions; ///< List of long options for `getopt_long`.
    std::vector<OptionInfo> _options;        ///< List of registered options.
    std::string _optString;                  ///< Option string for `getopt` (e.g., "vha:").

    /**
     * @brief Removes leading dashes from an option name.
     *
     * This helper function strips leading dashes (`-` or `--`) from an option name
     * to convert it into the format expected by `getopt_long`.
     *
     * @param name The option name to process.
     * @return A string without leading dashes.
     */
    [[nodiscard]] static std::string stripDashes(const std::string& name)
    {
        std::string stripped = name;
        while (!stripped.empty() && stripped[0] == '-')
        {
            stripped.erase(0, 1);
        }
        return stripped;
    }

  public:
    /**
     * @brief Adds an option to the parser.
     *
     * Registers an option with the parser, specifying its name, whether it requires
     * an argument, and its short name (if any).
     *
     * @param name The long name of the option (e.g., "--option").
     * @param hasArgument Indicates whether the option requires an argument.
     * @param shortName A single-character shorthand for the option (e.g., '-o').
     */
    void addOption(const std::string& name, bool hasArgument, char shortName) override
    {
        OptionInfo opt;
        opt.name = name;
        opt.shortName = shortName;
        opt.hasArgument = hasArgument;
        opt.isSet = false;
        _options.push_back(opt);

        if (shortName != '\0')
        {
            _optString += shortName;
            if (hasArgument)
            {
                _optString += ':';
            }
        }

        struct option longOpt = {0};
        longOpt.name = stripDashes(name).c_str();
        longOpt.has_arg = hasArgument ? required_argument : no_argument;
        longOpt.flag = nullptr;
        longOpt.val = shortName != '\0' ? shortName : 0;
        _longOptions.push_back(longOpt);
    }

    /**
     * @brief Adds a flag to the parser.
     *
     * Flags are options that do not require arguments (e.g., "--verbose").
     *
     * @param name The long name of the flag (e.g., "--flag").
     * @param shortName A single-character shorthand for the flag (e.g., '-f').
     */
    void addFlag(const std::string& name, char shortName) override
    {
        addOption(name, false, shortName);
    }

    /**
     * @brief Parses the command-line arguments.
     *
     * This method uses `getopt_long` to parse the provided arguments and updates
     * the internal state of the registered options.
     *
     * @param argc The number of arguments.
     * @param argv The array of argument strings.
     * @return `true` if parsing was successful, `false` otherwise.
     */
    [[nodiscard]] bool parse(int argc, char* argv[]) override
    {
        setError(ErrorCode::SUCCESS);

        struct option endOption = {0, 0, 0, 0};
        _longOptions.push_back(endOption);

        optind = 1;

        int longindex = 0;
        int c = 0;
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

            for (auto& opt : _options)
            {
                if (c == opt.shortName ||
                    (c != 0 && stripDashes(opt.name) == _longOptions[longindex].name))
                {
                    opt.isSet = true;

                    if (opt.hasArgument)
                    {
                        if (optarg)
                        {
                            opt.value = optarg;
                        }
                        else if (optind < argc && argv[optind][0] != '-')
                        {
                            opt.value = argv[optind];
                            optind++;
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

    /**
     * @brief Checks if a specific option was provided.
     *
     * @param name The name of the option to check (long or short name).
     * @return `true` if the option was provided, `false` otherwise.
     */
    [[nodiscard]] bool hasOption(const std::string& name) const override
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

    /**
     * @brief Retrieves the value of a specific option.
     *
     * @param name The name of the option (long or short name).
     * @return The value of the option as a `std::string`, or an empty string if
     *         the option was not provided or does not have a value.
     */
    [[nodiscard]] std::string getOptionValue(const std::string& name) const override
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

#endif // GETOPT_ARGUMENT_PARSER_HPP
