//
// Created by cj on 2026-06-14.
//

#include "converter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

#include "attribute_reader/attribute_reader.h"
#include "concurrent_writer.h"
#include "pclite_thread_pool.h"

Converter::Converter(std::vector<std::string> sources, std::string target, Options options)
    : sources_(std::move(sources)), target_(std::move(target)), options_(options) {}

void Converter::setProgressCallback(ConverterProgressCallback cb) {
    progressCallback_ = std::move(cb);
}

void Converter::reportProgress(const std::string& stage, float fraction) const {
    if (progressCallback_) progressCallback_(stage, fraction);
}

bool Converter::run() {
    writer_ = std::make_shared<ConcurrentWriter>(target_);

    if (!prepareAttributes()) return false;

    reportProgress("chunking", 0.f);
    if (!doChunking()) return false;
    reportProgress("chunking", 1.f);

    // Chunker wrote "chunks/<name>.bin" through writer_; flush+close those
    // streams now so Indexer's plain ifstream reads (in doSampling) see the
    // complete files.
    writer_->flushAll();

    reportProgress("hierarchy", 0.f);
    if (!buildHierarchy()) return false;
    reportProgress("hierarchy", 1.f);

    if (!doSampling()) return false; // reports its own per-chunk "sampling" progress

    reportProgress("merging", 0.f);
    if (!doMerging()) return false;
    reportProgress("merging", 1.f);

    reportProgress("rebuilding", 0.f);
    if (!doRebuildIndex()) return false;
    reportProgress("rebuilding", 1.f);

    writer_->flushAll();
    return true;
}

bool Converter::prepareAttributes() {
    readers_.clear();
    for (const std::string &source : sources_) {
        std::shared_ptr<AttributeReader> reader = AttributeReader::createReader(source);
        if (!reader) return false;
        readers_.push_back(reader);
    }

    if (readers_.empty()) return false;

    attributes_ = readers_[0]->getAttributes();

    std::set<std::string> names;
    for (const Attribute &attr : attributes_) names.insert(attr.name_);

    for (size_t i = 1; i < readers_.size(); ++i) {
        std::set<std::string> otherNames;
        for (const Attribute &attr : readers_[i]->getAttributes()) otherNames.insert(attr.name_);
        if (otherNames != names) return false;
    }

    return true;
}

bool Converter::doChunking() {
    chunker_ = std::make_unique<Chunker>(readers_, attributes_, target_, writer_, options_);
    if (!chunker_->run()) return false;

    chunks_ = chunker_->chunks();
    attributes_ = chunker_->attributes();
    return true;
}

bool Converter::buildHierarchy() {
    hierarchyBuilder_ = std::make_unique<HierarchyBuilder>(chunker_->aabb());
    hierarchyRoot_ = hierarchyBuilder_->build(chunks_, chunkRoots_);
    return hierarchyRoot_ != nullptr;
}

bool Converter::doSampling() {
    double rootSpacing = chunker_->aabb().getSize().x / static_cast<double>(chunker_->gridSize());

    indexer_ = std::make_unique<Indexer>(
        attributes_, target_, writer_,
        createSampler(options_.sampling, writer_, target_, attributes_, options_),
        options_, rootSpacing);

    size_t numThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
    PCLiteThreadPool pool(numThreads);

    std::vector<std::future<bool>> futures;
    futures.reserve(chunks_.size());
    for (size_t i = 0; i < chunks_.size(); ++i) {
        futures.push_back(
            pool.enqueue([this, i]() { return indexer_->indexChunk(chunkRoots_[i], chunks_[i]); }));
    }

    bool ok = true;
    size_t total = futures.size();
    for (size_t i = 0; i < futures.size(); ++i) {
        ok = futures[i].get() && ok;
        reportProgress("sampling", total > 0 ? static_cast<float>(i + 1) / static_cast<float>(total) : 1.f);
    }
    return ok;
}

bool Converter::doMerging() {
    return indexer_->mergeChunks(hierarchyRoot_, chunks_);
}

bool Converter::doRebuildIndex() {
    return indexer_->rebuildIndex();
}
