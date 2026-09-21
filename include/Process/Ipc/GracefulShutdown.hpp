// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_IPC_GRACEFUL_SHUTDOWN_HPP
#define PROCESS_IPC_GRACEFUL_SHUTDOWN_HPP

#include "Process/Ipc/ApiHandler.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

/// Drains in-flight requests once a shutdown is requested.
///
/// Lifecycle: requests are counted through RequestGuard; request() stops the listener on a
/// helper thread and waits for the count to reach zero, bounded by the timeout when there is
/// one; finish() joins that thread once the listener has returned.
class GracefulShutdown
{
  public:
    GracefulShutdown(ILogger& logger, std::chrono::milliseconds timeout);

    class RequestGuard
    {
      public:
        explicit RequestGuard(GracefulShutdown& owner);
        ~RequestGuard();
        RequestGuard(const RequestGuard&) = delete;
        RequestGuard& operator=(const RequestGuard&) = delete;

      private:
        GracefulShutdown& owner_;
    };

    [[nodiscard]] bool is_shutting_down() const noexcept;
    [[nodiscard]] bool is_requested() const noexcept;
    [[nodiscard]] std::chrono::milliseconds timeout() const noexcept;

    /// Starts the shutdown once. `stopListener` runs on the helper thread before the drain.
    /// Returns false when a shutdown was already requested.
    bool request(std::function<void()> stopListener);

    /// Joins the helper thread; call after the listener loop has returned.
    void finish();

  private:
    void begin_request();
    void end_request();
    void wait_for_drain();

    ILogger& logger_;
    std::chrono::milliseconds timeout_;
    std::atomic<bool> shutting_down_{false};
    std::atomic<bool> requested_{false};
    std::atomic<int> in_flight_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
};

#endif // PROCESS_IPC_GRACEFUL_SHUTDOWN_HPP
