#ifndef IANALYSISTOOLS_HPP
#define IANALYSISTOOLS_HPP

#include <iostream>
#include "Config/config.hpp"

namespace ctrace {

    /**
     * @brief Interface for an analysis tool (Strategy pattern).
     *
     * The `IAnalysisTool` class defines a common interface for all analysis tools.
     * It follows the Strategy design pattern, allowing different tools to be
     * implemented and used interchangeably.
     */
    class IAnalysisTool {
        public:
            /**
             * @brief Virtual destructor for the interface.
             *
             * Ensures proper cleanup of derived classes.
             */
            virtual ~IAnalysisTool() = default;

            /**
             * @brief Executes the analysis tool on a given file.
             *
             * This method runs the analysis tool on the specified file using the
             * provided program configuration.
             *
             * @param file The path to the file to analyze.
             * @param config The program configuration to use during the analysis.
             */
            virtual void execute(const std::string& file, ctrace::ProgramConfig config) const = 0;

            /**
             * @brief Retrieves the name of the analysis tool.
             *
             * @return A `std::string` representing the name of the tool.
             */
            virtual std::string name() const = 0;
    };

}

#endif // IANALYSISTOOLS_HPP
