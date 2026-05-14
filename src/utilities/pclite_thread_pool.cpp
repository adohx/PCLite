#include "pclite_thread_pool.h"

PCLiteThreadPool::PCLiteThreadPool(size_t numThreads)
    : pool_(numThreads) {}

void PCLiteThreadPool::waitAll() {
    std::unique_lock lock(waitMutex_);
    waitCv_.wait(lock, [this] {
        return pendingCount_.load(std::memory_order_acquire) == 0;
    });
}