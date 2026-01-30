#ifndef IARGUMENTPARSER_HPP
#define IARGUMENTPARSER_HPP

#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <memory>
#include <cstdio>
#include "CLI11.hpp"

/**
 * @brief Enum representing standardized error codes for argument parsing.
 *
 * The `ErrorCode` enum defines common error codes that can be returned
 * by an argument parser implementation.
 */
enum class ErrorCode : uint32_t
{
    SUCCESS = 0,          ///< Parsing completed successfully.
    INVALID_OPTION = 1,   ///< An invalid option was provided.
    MISSING_ARGUMENT = 2, ///< A required argument is missing.
    UNKNOWN_ERROR = 3     ///< An unknown error occurred.
};

/**
 * @brief Interface for argument parsing.
 *
 * The `IArgumentParser` class defines a pure virtual interface for parsing
 * command-line arguments. Implementations of this interface must provide
 * methods for adding options and flags, parsing arguments, and retrieving
 * parsed values or error information.
 */
class IArgumentParser
{
  public:
    /**
         * @brief Virtual destructor for the interface.
         *
         * Ensures proper cleanup of derived classes.
         */
    virtual ~IArgumentParser() = default;

    /**
         * @brief Adds an option to the parser.
         *
         * @param name The name of the option (e.g., "--option").
         * @param hasArgument Indicates whether the option requires an argument.
         * @param shortName A single-character shorthand for the option (e.g., '-o').
         */
    virtual void addOption(const std::string& name, bool hasArgument, char shortName) = 0;

    /**
         * @brief Adds a flag to the parser.
         *
         * Flags are options that do not require arguments (e.g., "--verbose").
         *
         * @param name The name of the flag (e.g., "--flag").
         * @param shortName A single-character shorthand for the flag (e.g., '-f').
         */
    virtual void addFlag(const std::string& name, char shortName) = 0;

    /**
         * @brief Parses the command-line arguments.
         *
         * @param argc The number of arguments.
         * @param argv The array of argument strings.
         * @return `true` if parsing was successful, `false` otherwise.
         */
    virtual bool parse(int argc, char* argv[]) = 0;

    /**
         * @brief Checks if a specific option was provided.
         *
         * @param name The name of the option to check.
         * @return `true` if the option was provided, `false` otherwise.
         */
    virtual bool hasOption(const std::string& name) const = 0;

    /**
         * @brief Retrieves the value of a specific option.
         *
         * @param name The name of the option.
         * @return The value of the option as a `std::string`.
         */
    virtual std::string getOptionValue(const std::string& name) const = 0;

    /**
         * @brief Retrieves the last error code.
         *
         * @return An `ErrorCode` representing the last error encountered.
         */
    virtual ErrorCode getLastError() const = 0;

    /**
         * @brief Retrieves the last error message.
         *
         * @return A `std::string` containing the error message.
         */
    virtual std::string getErrorMessage() const = 0;
};

#endif // IARGUMENTPARSER_HPP
