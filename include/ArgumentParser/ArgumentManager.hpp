#ifndef ARGUMENTMANAGER_HPP
#define ARGUMENTMANAGER_HPP

#include "IArgumentParser.hpp"
#include <memory>

/**
 * @brief Manages command-line argument parsing using an `IArgumentParser` implementation.
 *
 * The `ArgumentManager` class acts as a wrapper around an `IArgumentParser` instance,
 * providing a simplified interface for adding options, parsing arguments, and retrieving
 * parsed values.
 */
class ArgumentManager
{
    public:
        /**
         * @brief Constructs an `ArgumentManager` with a given argument parser.
         *
         * @param parser A `std::unique_ptr` to an `IArgumentParser` implementation.
         */
        explicit ArgumentManager(std::unique_ptr<IArgumentParser> parser);

        /**
         * @brief Adds an option to the argument parser.
         *
         * @param name The name of the option (e.g., "--option").
         * @param hasArgument Indicates whether the option requires an argument.
         * @param shortName A single-character shorthand for the option (e.g., '-o').
         */
        void addOption(const std::string& name, bool hasArgument, char shortName);

        /**
         * @brief Adds a flag to the argument parser.
         *
         * Flags are options that do not require arguments (e.g., "--verbose").
         *
         * @param name The name of the flag (e.g., "--flag").
         * @param shortName A single-character shorthand for the flag (e.g., '-f').
         */
        void addFlag(const std::string& name, char shortName);

        /**
         * @brief Parses the command-line arguments.
         *
         * @param argc The number of arguments.
         * @param argv The array of argument strings.
         */
        void parse(int argc, char* argv[]);

        /**
         * @brief Checks if a specific option was provided.
         *
         * @param name The name of the option to check.
         * @return `true` if the option was provided, `false` otherwise.
         */
        [[nodiscard]] bool hasOption(const std::string& name) const;

        /**
         * @brief Checks if no options were provided.
         *
         * @return `true` if no options were provided, `false` otherwise.
         */
        [[nodiscard]] bool hasNoOption() const;

        /**
         * @brief Retrieves the value of a specific option.
         *
         * @param name The name of the option.
         * @return The value of the option as a `std::string`.
         */
        [[nodiscard]] std::string getOptionValue(const std::string& name) const;

    private:
        std::unique_ptr<IArgumentParser> _parser; ///< The underlying argument parser implementation.
        bool _hasNoOption = false; ///< Flag indicating if no options were provided.
};

#endif // ARGUMENTMANAGER_HPP
