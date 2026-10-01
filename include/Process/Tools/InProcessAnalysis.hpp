// SPDX-License-Identifier: Apache-2.0
#ifndef IN_PROCESS_ANALYSIS_HPP
#define IN_PROCESS_ANALYSIS_HPP

#include <mutex>

namespace ctrace
{
    /// The analyzers linked into ctrace (stack, concurrency) compile their inputs through one
    /// coretrace-compiler, whose clang backend relies on process-wide state, and their own
    /// thread safety across concurrent runs is not established (on-disk caches, LLVM global
    /// state). Server mode handles requests on a thread pool and --async runs tool runs in
    /// parallel, so every in-process analysis runs under this one lock until the analyzers
    /// prove otherwise. Each is a batch tool, run once per invocation: the lock holds at most
    /// one pool worker per other in-process run.
    inline std::mutex& inProcessAnalysisMutex()
    {
        static std::mutex mutex;
        return mutex;
    }
} // namespace ctrace

#endif // IN_PROCESS_ANALYSIS_HPP
