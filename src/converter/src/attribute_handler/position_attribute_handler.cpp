//
// Created by cj on 2026-06-14.
//

#include "position_attribute_handler.h"

#include "attribute_codec.h"

void PositionAttributeHandler::encode(uint8_t *dst, const uint8_t *src,
                                       const Attribute &srcAttr, const Attribute &dstAttr) const {
    attribute_codec::encodeGeneric(dst, src, srcAttr, dstAttr);
}

void PositionAttributeHandler::decode(const uint8_t *src, const Attribute &attr, double *out) const {
    attribute_codec::decodeGeneric(src, attr, out);
}

void PositionAttributeHandler::updateStats(Attribute &attr, const uint8_t *rowsBegin,
                                            uint64_t numPoints, uint64_t rowStride) const {
    attribute_codec::updateRangeStats(attr, rowsBegin, numPoints, rowStride);
}

void PositionAttributeHandler::merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                                      size_t selectedIndex, const Attribute &attr) const {
    attribute_codec::mergeCopySelected(dstRow, srcRows, selectedIndex, attr);
}
