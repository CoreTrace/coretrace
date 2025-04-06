#ifndef BASE_ARGUMENT_PARSER_HPP
#define BASE_ARGUMENT_PARSER_HPP

#include "IArgumentParser.hpp"
#include "OptionInfo.hpp"

/**
 * @brief Base class for argument parsers.
 *
 * The `BaseArgumentParser` class provides a foundation for implementing
 * argument parsers. It inherits from the `IArgumentParser` interface and
 * includes common functionality such as error handling.
 */
class BaseArgumentParser : public IArgumentParser
{
    protected:
        ErrorCode _lastError; ///< Stores the last error code encountered.
        std::string _errorMessage; ///< Stores the last error message encountered.

        /**
         * @brief Sets the last error code and message.
         *
         * This method updates the internal error state with the provided
         * error code and optional message.
         *
         * @param code The error code to set.
         * @param message The error message to set (default is an empty string).
         */
        void setError(ErrorCode code, const std::string& message = "");

    public:
        /**
         * @brief Default constructor for `BaseArgumentParser`.
         *
         * Initializes the error state to `ErrorCode::SUCCESS` and clears
         * the error message.
         */
        BaseArgumentParser();

        /**
         * @brief Retrieves the last error code.
         *
         * @return An `ErrorCode` representing the last error encountered.
         */
        ErrorCode getLastError() const override;

        /**
         * @brief Retrieves the last error message.
         *
         * @return A `std::string` containing the last error message.
         */
        std::string getErrorMessage() const override;
};

#endif // BASE_ARGUMENT_PARSER_HPP
