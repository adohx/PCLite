//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_CHUNK_H
#define PCLITE_CHUNK_H
#include <memory>
#include <vector>
#include <string>

#include "attributes.h"
#include "vec3.h"

class Chunker {

public:
    explicit Chunker(const std::vector<std::string> &sources, const std::string &target);

    virtual ~Chunker();

    bool doChunking();
private:

private:
    friend class ChunkPrivate;
    struct NodeLut {
        uint64_t gridSize_;
        std::vector<uint64_t> grid_;
    };

    std::vector<std::string> sources_;
    std::string target_;

    class ChunkPrivate;
    std::unique_ptr<ChunkPrivate> d_;
};


#endif //PCLITE_CHUNK_H
