#ifndef CLI11_ARGUMENT_PARSER_HPP
#define CLI11_ARGUMENT_PARSER_HPP

#include "ArgumentParser/BaseArgumentParser.hpp"
#include "ArgumentParser/OptionInfo.hpp"
#include "../../../external/CLI11.hpp"
#include <vector>

/**
 * @brief Argument parser implementation using the CLI11 library.
 *
 * The `CLI11ArgumentParser` class provides an implementation of the `IArgumentParser`
 * interface using the CLI11 library for parsing command-line arguments. It supports
 * both short and long options, as well as options with or without arguments.
 */
class CLI11ArgumentParser : public BaseArgumentParser
{
  private:
    CLI::App app{"CLI11 Argument Parser"};             ///< CLI11 application instance.
    std::vector<OptionInfo> _options;                  ///< List of registered options.
    std::vector<std::pair<std::string, bool*>> _flags; ///< List of flag options (no arguments).
    std::vector<std::pair<std::string, std::string*>> _values; ///< List of options with arguments.

    /**
         * @brief Checks if an option already exists.
         *
         * This method verifies whether an option with the given name or short name
         * has already been registered.
         *
         * @param name The long name of the option.
         * @param shortName The short name of the option.
         * @return `true` if the option exists, `false` otherwise.
         */
    [[nodiscard]] bool optionExists(const std::string& name, char shortName) const
    {
        for (const auto& opt : _options)
        {
            if (opt.name == name || (shortName != '\0' && opt.shortName == shortName))
            {
                return true;
            }
        }
        return false;
    }

    /**
         * @brief Removes leading dashes from an option name.
         *
         * This helper function strips leading dashes (`-` or `--`) from an option name
         * to convert it into the format expected by CLI11.
         *
         * @param name The option name to process.
         * @return A string without leading dashes.
         */
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
    /**
     * @brief Constructs a `CLI11ArgumentParser` instance.
     *
     * Initializes the CLI11 application and disables the default help flag.
     */
    CLI11ArgumentParser()
    {
        app.set_help_flag(""); // Disable default help flag
    }

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
        if (optionExists(name, shortName))
        {
            setError(ErrorCode::INVALID_OPTION, "Option already exists: " + name);
            return;
        }

        OptionInfo opt;
        opt.name = name;
        opt.shortName = shortName;
        opt.hasArgument = hasArgument;
        opt.isSet = false;

        std::string longOpt = stripDashes(name);
        std::string shortOpt = shortName != '\0' ? std::string(1, shortName) : "";

        if (hasArgument)
        {
            auto* value = new std::string();
            _values.emplace_back(longOpt, value);
            auto* cliOption =
                app.add_option("-" + shortOpt + "," + name, *value, "Option with argument");
            if (!cliOption)
            {
                setError(ErrorCode::INVALID_OPTION, "Failed to add option: " + name);
            }
        }
        else
        {
            auto* flag = new bool(false);
            _flags.emplace_back(longOpt, flag);
            auto* cliFlag = app.add_flag("-" + shortOpt + "," + name, *flag, "Flag option");
            if (!cliFlag)
            {
                setError(ErrorCode::INVALID_OPTION, "Failed to add flag: " + name);
            }
        }
        _options.push_back(opt);
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
     * This method uses CLI11 to parse the provided arguments and updates the
     * internal state of the registered options.
     *
     * @param argc The number of arguments.
     * @param argv The array of argument strings.
     * @return `true` if parsing was successful, `false` otherwise.
     */
    bool parse(int argc, char* argv[]) override
    {
        setError(ErrorCode::SUCCESS);

        try
        {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& e)
        {
            if (e.get_exit_code() != 0)
            {
                setError(ErrorCode::INVALID_OPTION, e.what());
                return false;
            }
        }

        for (auto& opt : _options)
        {
            std::string longOpt = stripDashes(opt.name);
            if (opt.hasArgument)
            {
                for (const auto& [name, value] : _values)
                {
                    if (name == longOpt && !value->empty())
                    {
                        opt.isSet = true;
                        opt.value = *value;
                        break;
                    }
                }
            }
            else
            {
                for (const auto& [name, flag] : _flags)
                {
                    if (name == longOpt && *flag)
                    {
                        opt.isSet = true;
                        break;
                    }
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
    bool hasOption(const std::string& name) const override
    {
        for (const auto& opt : _options)
        {
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

    /**
     * @brief Destructor for `CLI11ArgumentParser`.
     *
     * Cleans up dynamically allocated memory for flags and values.
     */
    ~CLI11ArgumentParser() override
    {
        for (auto& [_, flag] : _flags)
        {
            delete flag;
        }
        for (auto& [_, value] : _values)
        {
            delete value;
        }
    }
};

#endif // CLI11_ARGUMENT_PARSER_HPP
