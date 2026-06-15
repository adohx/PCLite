//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_RGB_ATTRIBUTE_HANDLER_H
#define PCLITE_RGB_ATTRIBUTE_HANDLER_H

#include "attribute_handler.h"

// Handles the 3-channel "rgb" attribute. updateStats tracks the per-channel
// min_/max_ value range; merge follows the sampler's selectedIndex.
class RGBAttributeHandler : public AttributeHandler {
public:
    void encode(uint8_t *dst, const uint8_t *src,
                 const Attribute &srcAttr, const Attribute &dstAttr) const override;

    void decode(const uint8_t *src, const Attribute &attr, double *out) const override;

    void updateStats(Attribute &attr, const uint8_t *rowsBegin,
                      uint64_t numPoints, uint64_t rowStride) const override;

    void merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
               size_t selectedIndex, const Attribute &attr) const override;
};

#endif //PCLITE_RGB_ATTRIBUTE_HANDLER_H
