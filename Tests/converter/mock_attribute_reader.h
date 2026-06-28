//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_MOCK_ATTRIBUTE_READER_H
#define PCLITE_MOCK_ATTRIBUTE_READER_H

#include <vector>

#include "attribute_reader/attribute_reader.h"

// In-memory AttributeReader for tests: backed by a fixed list of world-space
// positions. readRawData() encodes those positions into the "position"
// attribute of `attributes` (via PositionAttributeHandler, so its scale_/offset_
// are honored) and zero-fills every other attribute's bytes.
class MockAttributeReader : public AttributeReader {
public:
    MockAttributeReader(Attributes attributes, std::vector<vec3d> positions);

    HeaderInfo headerInfo() override;
    vec3d readPosition(uint64_t index) override;
    std::vector<vec3d> readPositions(uint64_t index, int64_t counts) override;
    std::vector<uint8_t> readRawData(uint64_t index, int64_t count) override;
    ReaderType getType() override;

private:
    std::vector<vec3d> positions_;
};

#endif //PCLITE_MOCK_ATTRIBUTE_READER_H
