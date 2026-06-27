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

    // Opens relativePath in append mode, writes data, and closes immediately —
    // unlike append(), never keeps a long-lived std::ofstream open for it.
    // Use this when the number of distinct relativePath values can be very
    // large (e.g. per-batch hierarchy header files): append() holds one open
    // file handle per path until flushAll(), and closing tens of thousands of
    // them at once becomes the dominant cost. Concurrent calls for the same
    // relativePath are still serialized (and ordered), via a lightweight
    // per-path mutex that doesn't itself hold a file handle open.
    void appendAndClose(const std::string &relativePath, const std::vector<uint8_t> &data);

    // Same as append(), but runs on the thread pool.
    std::future<WriteResult> appendAsync(const std::string &relativePath, std::vector<uint8_t> data);

    // Overwrite bytes at a fixed offset (e.g. to backfill a node's byteSize_ after it is known).
    void writeAt(const std::string &relativePath, uint64_t offset, const std::vector<uint8_t> &data);

    // Block until all pending appendAsync tasks finish, then close all file handles.
    void flushAll();

    // Flushes one file's stream WITHOUT closing it, so its on-disk content is
    // safe to read back via a separate ifstream while later append() calls to
    // the same path keep working (unlike flushAll(), which closes every
    // stream and breaks subsequent writes to already-registered files — see
    // feedback_concurrent_writer memory). No-op if relativePath was never
    // written to.
    void flush(const std::string &relativePath);

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

    // Per-path mutexes for appendAndClose() — separate from `files_` since
    // these paths never get a long-lived FileState/ofstream.
    std::mutex ephemeralRegistryMutex_;
    std::unordered_map<std::string, std::unique_ptr<std::mutex>> ephemeralMutexes_;
};

#endif //PCLITE_CONCURRENT_WRITER_H
