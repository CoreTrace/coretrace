// SPDX-License-Identifier: Apache-2.0
#include "Process/Ipc/GracefulShutdown.hpp"

#include <utility>

GracefulShutdown::GracefulShutdown(ILogger& logger, std::chrono::milliseconds timeout)
    : logger_(logger), timeout_(timeout)
{
}

GracefulShutdown::RequestGuard::RequestGuard(GracefulShutdown& owner) : owner_(owner)
{
    owner_.begin_request();
}

GracefulShutdown::RequestGuard::~RequestGuard()
{
    owner_.end_request();
}

bool GracefulShutdown::is_shutting_down() const noexcept
{
    return shutting_down_.load(std::memory_order_acquire);
}

bool GracefulShutdown::is_requested() const noexcept
{
    return requested_.load(std::memory_order_acquire);
}

std::chrono::milliseconds GracefulShutdown::timeout() const noexcept
{
    return timeout_;
}

void GracefulShutdown::begin_request()
{
    in_flight_.fetch_add(1, std::memory_order_relaxed);
}

void GracefulShutdown::end_request()
{
    const int remaining = in_flight_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining == 0)
    {
        cv_.notify_all();
    }
}

bool GracefulShutdown::request(std::function<void()> stopListener)
{
    bool expected = false;
    if (!requested_.compare_exchange_strong(expected, true))
    {
        return false;
    }

    shutting_down_.store(true, std::memory_order_release);
    thread_ = std::thread(
        [this, stop = std::move(stopListener)]()
        {
            logger_.info("[SERVER] Shutdown requested. Stopping listener...");
            stop();
            wait_for_drain();
        });
    return true;
}

void GracefulShutdown::wait_for_drain()
{
    std::unique_lock<std::mutex> lock(mutex_);
    const auto done = [this]() { return in_flight_.load(std::memory_order_acquire) == 0; };

    if (timeout_.count() > 0)
    {
        if (!cv_.wait_for(lock, timeout_, done))
        {
            logger_.error("[SERVER] Shutdown timeout exceeded. Forcing exit.");
        }
    }
    else
    {
        cv_.wait(lock, done);
    }
}

void GracefulShutdown::finish()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
    if (is_requested())
    {
        logger_.info("[SERVER] Shutdown complete.");
    }
}
