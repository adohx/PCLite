//
// Created by cj on 2026-06-14.
//

#include "concurrent_writer.h"

#include <filesystem>
#include <stdexcept>

ConcurrentWriter::ConcurrentWriter(std::string targetDir, size_t numThreads)
    : targetDir_(std::move(targetDir)), pool_(numThreads) {
    std::filesystem::create_directories(targetDir_);
}

std::string ConcurrentWriter::fullPath(const std::string &relativePath) const {
    return (std::filesystem::path(targetDir_) / relativePath).string();
}

ConcurrentWriter::FileState &ConcurrentWriter::fileState(const std::string &relativePath) {
    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = files_.find(relativePath);
    if (it != files_.end()) {
        return *it->second;
    }

    auto path = fullPath(relativePath);
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    auto state = std::make_unique<FileState>();
    state->stream.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!state->stream) {
        throw std::runtime_error("ConcurrentWriter: failed to open '" + path + "' for writing");
    }

    auto [inserted, _] = files_.emplace(relativePath, std::move(state));
    return *inserted->second;
}

ConcurrentWriter::WriteResult ConcurrentWriter::append(const std::string &relativePath,
                                                         const std::vector<uint8_t> &data) {
    FileState &state = fileState(relativePath);
    std::lock_guard<std::mutex> lock(state.mutex);

    WriteResult result{state.cursor, data.size()};
    if (!data.empty()) {
        state.stream.seekp(static_cast<std::streamoff>(state.cursor));
        state.stream.write(reinterpret_cast<const char *>(data.data()),
                            static_cast<std::streamsize>(data.size()));
    }
    state.cursor += data.size();
    return result;
}

void ConcurrentWriter::appendAndClose(const std::string &relativePath, const std::vector<uint8_t> &data) {
    std::mutex *pathMutex;
    {
        std::lock_guard<std::mutex> lock(ephemeralRegistryMutex_);
        auto it = ephemeralMutexes_.find(relativePath);
        if (it == ephemeralMutexes_.end()) {
            it = ephemeralMutexes_.emplace(relativePath, std::make_unique<std::mutex>()).first;
        }
        pathMutex = it->second.get();
    }

    std::lock_guard<std::mutex> lock(*pathMutex);

    auto path = fullPath(relativePath);
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) {
        throw std::runtime_error("ConcurrentWriter: failed to open '" + path + "' for append");
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    }
}

std::future<ConcurrentWriter::WriteResult> ConcurrentWriter::appendAsync(const std::string &relativePath,
                                                                          std::vector<uint8_t> data) {
    return pool_.enqueue([this, relativePath, data = std::move(data)]() mutable {
        return append(relativePath, data);
    });
}

void ConcurrentWriter::writeAt(const std::string &relativePath, uint64_t offset,
                                const std::vector<uint8_t> &data) {
    FileState &state = fileState(relativePath);
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!data.empty()) {
        state.stream.seekp(static_cast<std::streamoff>(offset));
        state.stream.write(reinterpret_cast<const char *>(data.data()),
                            static_cast<std::streamsize>(data.size()));
    }
    uint64_t end = offset + data.size();
    if (end > state.cursor) {
        state.cursor = end;
    }
}

void ConcurrentWriter::flush(const std::string &relativePath) {
    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = files_.find(relativePath);
    if (it == files_.end()) return;

    std::lock_guard<std::mutex> fileLock(it->second->mutex);
    it->second->stream.flush();
}

void ConcurrentWriter::flushAll() {
    pool_.waitAll();

    std::lock_guard<std::mutex> lock(registryMutex_);
    for (auto &[path, state] : files_) {
        std::lock_guard<std::mutex> fileLock(state->mutex);
        state->stream.flush();
        state->stream.close();
    }
}
