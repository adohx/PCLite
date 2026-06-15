//
// Created by cj on 2026-06-14.
//

#include "mock_attribute_reader.h"

#include <algorithm>
#include <limits>

#include "attribute_handler/attribute_codec.h"

MockAttributeReader::MockAttributeReader(Attributes attributes, std::vector<vec3d> positions)
    : AttributeReader("<mock>"), positions_(std::move(positions)) {
    attributes_ = std::move(attributes);
}

AttributeReader::HeaderInfo MockAttributeReader::headerInfo() {
    constexpr double kInf = std::numeric_limits<double>::infinity();

    HeaderInfo info;
    info.name_ = path_;
    info.description_ = "";
    info.numPoints_ = positions_.size();
    info.extendedNumPoints_ = positions_.size();
    info.bpp_ = static_cast<int>(attributes_.getTotalBytes());

    vec3d lo{kInf, kInf, kInf};
    vec3d hi{-kInf, -kInf, -kInf};
    for (const vec3d &p : positions_) {
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
        hi.z = std::max(hi.z, p.z);
    }
    if (positions_.empty()) {
        lo = hi = vec3d{0, 0, 0};
    }
    info.min_ = lo;
    info.max_ = hi;

    return info;
}

vec3d MockAttributeReader::readPosition(uint64_t index) {
    return positions_[index];
}

std::vector<vec3d> MockAttributeReader::readPositions(uint64_t index, int64_t counts) {
    auto begin = positions_.begin() + static_cast<int64_t>(index);
    return std::vector<vec3d>(begin, begin + counts);
}

std::vector<uint8_t> MockAttributeReader::readRawData(uint64_t index, int64_t count) {
    uint64_t rowBytes = attributes_.getTotalBytes();
    std::vector<uint8_t> rows(static_cast<size_t>(count) * rowBytes, 0);

    Attribute posAttr = attributes_.getAttribute("position");
    uint64_t posOffset = attributes_.getOffset("position");
    int elemBytes = posAttr.numElements_ > 0 ? posAttr.bytes_ / posAttr.numElements_ : 0;

    for (int64_t i = 0; i < count; ++i) {
        const vec3d &p = positions_[index + static_cast<uint64_t>(i)];
        uint8_t *row = rows.data() + static_cast<uint64_t>(i) * rowBytes + posOffset;

        for (int c = 0; c < posAttr.numElements_; ++c) {
            double world = attribute_codec::component(p, c);
            double scale = attribute_codec::component(posAttr.scale_, c);
            double offset = attribute_codec::component(posAttr.offset_, c);
            double raw = (scale != 0.0) ? (world - offset) / scale : world - offset;
            attribute_codec::writeElement(row + c * elemBytes, posAttr.type_, raw);
        }
    }

    return rows;
}

AttributeReader::ReaderType MockAttributeReader::getType() {
    return PCD;
}
