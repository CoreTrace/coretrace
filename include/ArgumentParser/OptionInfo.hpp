#ifndef OPTIONINFO_HPP
#define OPTIONINFO_HPP

#include <string>

/**
 * @brief Represents information about a command-line option.
 *
 * The `OptionInfo` struct is used to store metadata and state for a single
 * command-line option, including its name, shorthand, whether it requires
 * an argument, and its current value.
 */
struct OptionInfo
{
    std::string name;  ///< Long name of the option (e.g., "--verbose").
    char shortName;    ///< Short name of the option (e.g., '-v').
    bool hasArgument;  ///< Indicates whether the option requires an argument.
    bool isSet;        ///< Indicates whether the option was specified.
    std::string value; ///< The value associated with the option (if `hasArgument` is `true`).
};

#endif // OPTIONINFO_HPP
