#ifndef THREAD_PROCESS_HPP
#define THREAD_PROCESS_HPP

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ctrace
{
    namespace Thread
    {
        namespace Output
        {
            struct CapturedLine
            {
                std::string stream;
                std::string message;
            };

            class CaptureBuffer
            {
              public:
                void append(const std::string& tool, const std::string& stream,
                            const std::string& message)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    lines_[tool].push_back({stream, message});
                }

                std::unordered_map<std::string, std::vector<CapturedLine>> snapshot() const
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    return lines_;
                }

              private:
                mutable std::mutex mutex_;
                std::unordered_map<std::string, std::vector<CapturedLine>> lines_;
            };

            struct CaptureContext
            {
                std::shared_ptr<CaptureBuffer> buffer;
                std::string tool;
                bool mirror_to_console = true;
            };

            inline std::mutex cout_mutex;
            inline thread_local const CaptureContext* capture_context = nullptr;

            class ScopedCapture
            {
              public:
                explicit ScopedCapture(const CaptureContext* ctx)
                    : previous_(capture_context), active_(ctx != nullptr)
                {
                    if (active_)
                    {
                        capture_context = ctx;
                    }
                }

                ~ScopedCapture()
                {
                    if (active_)
                    {
                        capture_context = previous_;
                    }
                }

              private:
                const CaptureContext* previous_;
                bool active_;
            };

            static void emit_line(const std::string& stream, const std::string& message,
                                  std::ostream& target, bool capture_line)
            {
                const CaptureContext* ctx = capture_context;
                if (capture_line && ctx && ctx->buffer)
                {
                    ctx->buffer->append(ctx->tool, stream, message);
                    if (!ctx->mirror_to_console)
                    {
                        return;
                    }
                }
                std::lock_guard<std::mutex> lock(cout_mutex);
                target << message << std::endl;
            }

            static void cout(const std::string& message)
            {
                emit_line("stdout", message, std::cout, false);
            }

            static void cerr(const std::string& message)
            {
                emit_line("stderr", message, std::cerr, false);
            }

            static void tool_out(const std::string& message)
            {
                emit_line("stdout", message, std::cout, true);
            }

            static void tool_err(const std::string& message)
            {
                emit_line("stderr", message, std::cerr, true);
            }
        } // namespace Output
    }     // namespace Thread
} // namespace ctrace

#endif // THREAD_PROCESS_HPP
