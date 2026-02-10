#include "main_thread_dispatcher.h"

#include <algorithm>
#include <utility>

MainThreadDispatcher& MainThreadDispatcher::instance()
{
    static MainThreadDispatcher dispatcher;
    return dispatcher;
}

void MainThreadDispatcher::SetMainThread(std::thread::id id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    main_thread_id_ = id;
}

bool MainThreadDispatcher::IsMainThread() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return main_thread_id_ != std::thread::id() && std::this_thread::get_id() == main_thread_id_;
}

void MainThreadDispatcher::Post(std::function<void()> task, Priority priority)
{
    if (!task)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (priority == Priority::High)
    {
        high_priority_queue_.push_back(std::move(task));
    }
    else
    {
        normal_priority_queue_.push_back(std::move(task));
    }
}

void MainThreadDispatcher::Post(std::function<void()> task)
{
    Post(std::move(task), Priority::Normal);
}

void MainThreadDispatcher::Drain()
{
    Drain(32, 256);
}

void MainThreadDispatcher::Drain(std::size_t normal_base_budget, std::size_t normal_max_budget)
{
    std::deque<std::function<void()>> high_priority_tasks;
    std::deque<std::function<void()>> normal_priority_tasks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        high_priority_tasks.swap(high_priority_queue_);

        const std::size_t queued_normals = normal_priority_queue_.size();
        const std::size_t boosted_budget = normal_base_budget + queued_normals / 4;
        const std::size_t normal_budget = std::max<std::size_t>(1, std::min(normal_max_budget, boosted_budget));
        const std::size_t normal_tasks_to_run = std::min(normal_priority_queue_.size(), normal_budget);

        for (std::size_t i = 0; i < normal_tasks_to_run; ++i)
        {
            normal_priority_tasks.push_back(std::move(normal_priority_queue_.front()));
            normal_priority_queue_.pop_front();
        }
    }

    for (auto& task : high_priority_tasks)
    {
        task();
    }

    for (auto& task : normal_priority_tasks)
    {
        task();
    }
}

void MainThreadDispatcher::DrainAll()
{
    while (true)
    {
        std::deque<std::function<void()>> high_priority_tasks;
        std::deque<std::function<void()>> normal_priority_tasks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (high_priority_queue_.empty() && normal_priority_queue_.empty())
            {
                return;
            }
            high_priority_tasks.swap(high_priority_queue_);
            normal_priority_tasks.swap(normal_priority_queue_);
        }

        for (auto& task : high_priority_tasks)
        {
            task();
        }
        for (auto& task : normal_priority_tasks)
        {
            task();
        }
    }
}

bool MainThreadDispatcher::HasPendingTasks() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !high_priority_queue_.empty() || !normal_priority_queue_.empty();
}

std::size_t MainThreadDispatcher::PendingTaskCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return high_priority_queue_.size() + normal_priority_queue_.size();
}
