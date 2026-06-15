//
// Created by cj on 2026-06-14.
//

#include "classification_attribute_handler.h"

#include <algorithm>

#include "attribute_codec.h"

void ClassificationAttributeHandler::updateStats(Attribute &attr, const uint8_t *rowsBegin,
                                                  uint64_t numPoints, uint64_t rowStride) const {
    ScalarAttributeHandler::updateStats(attr, rowsBegin, numPoints, rowStride);

    if (attr.histogram_.size() < 256) {
        attr.histogram_.resize(256, 0);
    }

    for (uint64_t i = 0; i < numPoints; ++i) {
        const uint8_t *row = rowsBegin + i * rowStride;
        double raw = attribute_codec::readElement(row, attr.type_);
        int bin = static_cast<int>(std::clamp(raw, 0.0, 255.0));
        attr.histogram_[bin]++;
    }
}
