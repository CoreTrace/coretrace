// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_INTERRUPT_HPP
#define PROCESS_INTERRUPT_HPP

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

/// SIGINT and SIGTERM. The handler only records the signal; the code that waits on tools and
/// requests acts on it: a run stops its tools, so that their run directories are removed, and a
/// server drains as POST /shutdown does. ctrace then ends by the same signal (exitBy). A second
/// interrupt ends ctrace at once.
namespace ctrace::interrupt
{
    /// What a first interrupt does to the tools running at that moment.
    enum class OnInterrupt
    {
        StopTools,      ///< A run: what it would report is incomplete anyway.
        LetToolsFinish, ///< A server: the requests in flight are drained, then answered.
    };

    namespace detail
    {
        inline std::atomic<int> received{0};
        inline std::atomic<bool> stopsTools{false};

        inline void onSignal(int signal)
        {
            int none = 0;
            if (!received.compare_exchange_strong(none, signal))
            {
                std::signal(signal, SIG_DFL);
                std::raise(signal);
                return;
            }
            constexpr char kMessage[] =
                "\nInterrupted: ctrace is stopping. Interrupt again to quit at once.\n";
            // Nothing to do if stderr is gone: the interrupt still takes effect.
            [[maybe_unused]] const ssize_t written =
                ::write(STDERR_FILENO, kMessage, sizeof(kMessage) - 1);
        }
    } // namespace detail

    /// Installs the handler for SIGINT and SIGTERM. A signal ignored when ctrace started stays
    /// ignored, as the background jobs of a script expect.
    inline void install(OnInterrupt onInterrupt)
    {
        detail::stopsTools.store(onInterrupt == OnInterrupt::StopTools);
        for (const int signal : {SIGINT, SIGTERM})
        {
            struct sigaction previous{};
            if (::sigaction(signal, nullptr, &previous) == 0 && previous.sa_handler == SIG_IGN)
            {
                continue;
            }
            struct sigaction action{};
            action.sa_handler = detail::onSignal;
            sigemptyset(&action.sa_mask);
            action.sa_flags = SA_RESTART;
            (void)::sigaction(signal, &action, nullptr);
        }
    }

    /// The signal that interrupted ctrace; 0 when none did.
    [[nodiscard]] inline int received() noexcept
    {
        return detail::received.load();
    }

    /// Whether the running tools must stop now.
    [[nodiscard]] inline bool toolsMustStop() noexcept
    {
        return received() != 0 && detail::stopsTools.load();
    }

    /// The shell's exit status of a command that `signal` ended.
    [[nodiscard]] constexpr int exitCode(int signal) noexcept
    {
        return 128 + signal;
    }

    /// Ends ctrace by `signal`, once it has cleaned up: the shell that started it sees an
    /// interrupted command, so that a script loop over ctrace stops too.
    [[noreturn]] inline void exitBy(int signal)
    {
        std::cout.flush();
        std::cerr.flush();
        std::signal(signal, SIG_DFL);
        std::raise(signal);
        std::_Exit(exitCode(signal));
    }
} // namespace ctrace::interrupt

#endif // PROCESS_INTERRUPT_HPP
