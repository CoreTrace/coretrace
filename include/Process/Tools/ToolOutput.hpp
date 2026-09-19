// SPDX-License-Identifier: Apache-2.0
#ifndef TOOL_OUTPUT_HPP
#define TOOL_OUTPUT_HPP

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ctrace
{
    struct CapturedLine
    {
        std::string stream; ///< "stdout" or "stderr".
        std::string message;
    };

    /// Thread-safe store of what each tool produced, keyed by tool name. The HTTP server turns
    /// it into `result.outputs`.
    class CaptureBuffer
    {
      public:
        void append(const std::string& tool, const std::string& stream, const std::string& message)
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            lines_[tool].push_back({stream, message});
        }

        [[nodiscard]] std::unordered_map<std::string, std::vector<CapturedLine>> snapshot() const
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            return lines_;
        }

      private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, std::vector<CapturedLine>> lines_;
    };

    /// The sink a tool writes to during one execution. Results and errors are recorded in the
    /// capture buffer (when there is one) and mirrored to stdout/stderr (when asked to).
    /// Status and progress messages are not tool output: they go through the logger.
    class ToolOutput
    {
      public:
        ToolOutput(std::shared_ptr<CaptureBuffer> buffer, std::string tool, bool mirrorToConsole)
            : buffer_(std::move(buffer)), tool_(std::move(tool)), mirror_(mirrorToConsole)
        {
        }

        /// The tool's report text.
        void result(const std::string& text)
        {
            record("stdout", text);
            if (mirror_)
            {
                print(std::cout, text);
            }
        }

        /// A failure of the tool (missing binary, non-zero exit, analyzer error).
        void error(const std::string& text)
        {
            record("stderr", text);
            if (mirror_)
            {
                print(std::cerr, text);
            }
        }

        /// Records without printing, for output that reached the user another way (IPC).
        void record(const std::string& stream, const std::string& text)
        {
            if (buffer_ && !text.empty())
            {
                buffer_->append(tool_, stream, text);
            }
        }

        [[nodiscard]] const std::string& tool() const noexcept
        {
            return tool_;
        }

      private:
        static void print(std::ostream& target, const std::string& text)
        {
            static std::mutex consoleMutex;
            const std::lock_guard<std::mutex> lock(consoleMutex);
            target << text << std::endl;
        }

        std::shared_ptr<CaptureBuffer> buffer_;
        std::string tool_;
        bool mirror_;
    };
} // namespace ctrace

#endif // TOOL_OUTPUT_HPP
