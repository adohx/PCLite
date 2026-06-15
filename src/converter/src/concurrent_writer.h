//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_CONCURRENT_WRITER_H
#define PCLITE_CONCURRENT_WRITER_H

#include <cstdint>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "pclite_thread_pool.h"

// Manages concurrent append/overwrite writes to files under a target directory.
// Each relative path gets its own write cursor and mutex, so writes to
// different files proceed independently while writes to the same file are
// serialized (and therefore ordered by call order for append()).
class ConcurrentWriter {
public:
    struct WriteResult {
        uint64_t offset;  // start offset within the file, relative to file start
        uint64_t size;    // number of bytes written
    };

    explicit ConcurrentWriter(std::string targetDir,
                               size_t numThreads = std::thread::hardware_concurrency());

    // Synchronously append data to the end of relativePath, returning where it landed.
    // Concurrent calls for the same relativePath are serialized in call order.
    WriteResult append(const std::string &relativePath, const std::vector<uint8_t> &data);

    // Same as append(), but runs on the thread pool.
    std::future<WriteResult> appendAsync(const std::string &relativePath, std::vector<uint8_t> data);

    // Overwrite bytes at a fixed offset (e.g. to backfill a node's byteSize_ after it is known).
    void writeAt(const std::string &relativePath, uint64_t offset, const std::vector<uint8_t> &data);

    // Block until all pending appendAsync tasks finish, then close all file handles.
    void flushAll();

private:
    struct FileState {
        std::ofstream stream;
        std::mutex mutex;
        uint64_t cursor = 0;
    };

    FileState &fileState(const std::string &relativePath);
    std::string fullPath(const std::string &relativePath) const;

    std::string targetDir_;
    PCLiteThreadPool pool_;
    std::mutex registryMutex_;
    std::unordered_map<std::string, std::unique_ptr<FileState>> files_;
};

#endif //PCLITE_CONCURRENT_WRITER_H
