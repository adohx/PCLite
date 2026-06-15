//
// Created by cj on 2026-06-14.
//

#include "scalar_attribute_handler.h"

#include "attribute_codec.h"

void ScalarAttributeHandler::encode(uint8_t *dst, const uint8_t *src,
                                     const Attribute &srcAttr, const Attribute &dstAttr) const {
    attribute_codec::encodeGeneric(dst, src, srcAttr, dstAttr);
}

void ScalarAttributeHandler::decode(const uint8_t *src, const Attribute &attr, double *out) const {
    attribute_codec::decodeGeneric(src, attr, out);
}

void ScalarAttributeHandler::updateStats(Attribute &attr, const uint8_t *rowsBegin,
                                          uint64_t numPoints, uint64_t rowStride) const {
    attribute_codec::updateRangeStats(attr, rowsBegin, numPoints, rowStride);
}

void ScalarAttributeHandler::merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                                    size_t selectedIndex, const Attribute &attr) const {
    attribute_codec::mergeCopySelected(dstRow, srcRows, selectedIndex, attr);
}
