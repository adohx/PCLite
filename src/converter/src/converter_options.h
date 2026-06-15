//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_CONVERTER_OPTIONS_H
#define PCLITE_CONVERTER_OPTIONS_H

#include <cstdint>
#include <string>

// Shared by Converter, Chunker, HierarchyBuilder and Indexer. Kept in its own
// header (rather than nested inside Converter) so those classes don't need to
// include converter.h.
struct ConverterOptions {
    uint64_t maxPointsPerChunk = 0;    // chunk size threshold; 0 = derive from total point count
    uint32_t minGridSize = 0;          // finest pre-chunking grid size (power of 2); 0 = auto
    uint32_t firstChunkSize = 4096;    // hierarchy pyramid parameter, written to metadata.hierarchy
    uint32_t stepSize = 4;             // hierarchy pyramid parameter, written to metadata.hierarchy
    std::string sampling = "poisson";  // sampler name, used by Sampler::create
};

#endif //PCLITE_CONVERTER_OPTIONS_H
