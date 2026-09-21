// SPDX-License-Identifier: Apache-2.0
#ifndef IANALYSISTOOLS_HPP
#define IANALYSISTOOLS_HPP

#include <iostream>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

#include "Config/config.hpp"
#include "Process/Tools/Diagnostic.hpp"
#include "Process/Tools/ToolOutput.hpp"

class IpcStrategy;

namespace ctrace
{
    /**
     * @brief Interface for an analysis tool (Strategy pattern).
     *
     * The `IAnalysisTool` class defines a common interface for all analysis tools.
     * It follows the Strategy design pattern, allowing different tools to be
     * implemented and used interchangeably.
     */
    class IAnalysisTool
    {
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
        virtual void execute(const std::string& file, const ctrace::ProgramConfig& config,
                             ToolOutput& output) const = 0;

        /**
             * @brief Indicates whether the tool can process multiple inputs in one run.
             *
             * Tools returning true will be scheduled once with the full resolved input list.
             * Default behavior keeps per-file execution.
             */
        [[nodiscard]] virtual bool supportsBatchExecution() const
        {
            return false;
        }

        /**
             * @brief Executes the analysis tool on multiple files in one run.
             *
             * Default implementation falls back to per-file execution.
             *
             * @param files The list of files to analyze.
             * @param config The program configuration to use during the analysis.
             */
        virtual void executeBatch(const std::vector<std::string>& files,
                                  const ctrace::ProgramConfig& config, ToolOutput& output) const
        {
            for (const auto& file : files)
            {
                execute(file, config, output);
            }
        }

        /**
             * @brief Retrieves the name of the analysis tool.
             *
             * @return A `std::string` representing the name of the tool.
             */
        virtual std::string name() const = 0;

        /**
             * @brief Sets the IPC strategy for the analysis tool.
             * This method allows the tool to communicate results or data
             * through the specified IPC mechanism.
             * @param ipc A shared pointer to an `IpcStrategy` instance.
             */
        virtual void setIpcStrategy(std::shared_ptr<IpcStrategy> ipc) = 0;
    };

} // namespace ctrace

#endif // IANALYSISTOOLS_HPP
