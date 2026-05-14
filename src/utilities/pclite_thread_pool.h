#pragma once

#include "3rd_party/ThreadPool/ThreadPool.h"
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>

class PCLiteThreadPool {
public:
    explicit PCLiteThreadPool(size_t numThreads);

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    // 阻塞直到所有已提交的任务全部完成
    void waitAll();

private:
    ThreadPool pool_;
    std::atomic<int> pendingCount_{0};
    std::mutex waitMutex_;
    std::condition_variable waitCv_;
};

template <class F, class... Args>
auto PCLiteThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    pendingCount_.fetch_add(1, std::memory_order_relaxed);

    auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    return pool_.enqueue([this, task = std::move(bound)]() mutable {
        // RAII guard: 任务结束时（无论正常还是异常）自动递减计数并通知
        struct Guard {
            PCLiteThreadPool* self;
            ~Guard() {
                if (self->pendingCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard lock(self->waitMutex_);
                    self->waitCv_.notify_all();
                }
            }
        } guard{this};

        return task();
    });
}