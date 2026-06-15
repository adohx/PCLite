//
// Created by cj on 2026-06-14.
//

#include "rgb_attribute_handler.h"

#include "attribute_codec.h"

void RGBAttributeHandler::encode(uint8_t *dst, const uint8_t *src,
                                  const Attribute &srcAttr, const Attribute &dstAttr) const {
    attribute_codec::encodeGeneric(dst, src, srcAttr, dstAttr);
}

void RGBAttributeHandler::decode(const uint8_t *src, const Attribute &attr, double *out) const {
    attribute_codec::decodeGeneric(src, attr, out);
}

void RGBAttributeHandler::updateStats(Attribute &attr, const uint8_t *rowsBegin,
                                       uint64_t numPoints, uint64_t rowStride) const {
    attribute_codec::updateRangeStats(attr, rowsBegin, numPoints, rowStride);
}

void RGBAttributeHandler::merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                                 size_t selectedIndex, const Attribute &attr) const {
    attribute_codec::mergeCopySelected(dstRow, srcRows, selectedIndex, attr);
}
