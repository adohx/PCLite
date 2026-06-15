//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_SCALAR_ATTRIBUTE_HANDLER_H
#define PCLITE_SCALAR_ATTRIBUTE_HANDLER_H

#include "attribute_handler.h"

// Generic numeric attribute (intensity, return number, number of returns,
// scan angle rank, user data, point source id, ...): reads/writes a
// fixed-width integer/float, and tracks min_/max_ over its component(s).
class ScalarAttributeHandler : public AttributeHandler {
public:
    void encode(uint8_t *dst, const uint8_t *src,
                 const Attribute &srcAttr, const Attribute &dstAttr) const override;

    void decode(const uint8_t *src, const Attribute &attr, double *out) const override;

    void updateStats(Attribute &attr, const uint8_t *rowsBegin,
                      uint64_t numPoints, uint64_t rowStride) const override;

    void merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
               size_t selectedIndex, const Attribute &attr) const override;
};

#endif //PCLITE_SCALAR_ATTRIBUTE_HANDLER_H
