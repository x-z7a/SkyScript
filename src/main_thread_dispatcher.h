#pragma once

#include <deque>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

class MainThreadDispatcher
{
public:
    enum class Priority : uint8_t
    {
        High,
        Normal,
    };

    static MainThreadDispatcher& instance();

    void SetMainThread(std::thread::id id = std::this_thread::get_id());
    bool IsMainThread() const;

    // Backward-compatible normal-priority enqueue.
    void Post(std::function<void()> task);
    void Post(std::function<void()> task, Priority priority);
    // Backward-compatible default-budget drain.
    void Drain();
    void Drain(std::size_t normal_base_budget, std::size_t normal_max_budget);
    void DrainAll();
    bool HasPendingTasks() const;
    std::size_t PendingTaskCount() const;

    template <typename Fn>
    auto CallSync(Fn&& fn) -> decltype(fn())
    {
        using ReturnType = decltype(fn());
        if (IsMainThread())
        {
            return fn();
        }

        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto future = promise->get_future();

        Post([promise, task = std::forward<Fn>(fn)]() mutable {
                 try
                 {
                     if constexpr (std::is_void_v<ReturnType>)
                     {
                         task();
                         promise->set_value();
                     }
                     else
                     {
                         promise->set_value(task());
                     }
                 }
                 catch (...)
                 {
                     promise->set_exception(std::current_exception());
                 } },
             Priority::High);

        if constexpr (std::is_void_v<ReturnType>)
        {
            future.get();
            return;
        }
        else
        {
            return future.get();
        }
    }

private:
    MainThreadDispatcher() = default;

    mutable std::mutex mutex_;
    std::deque<std::function<void()>> high_priority_queue_;
    std::deque<std::function<void()>> normal_priority_queue_;
    std::thread::id main_thread_id_;
};
