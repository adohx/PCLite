//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_CLASSIFICATION_ATTRIBUTE_HANDLER_H
#define PCLITE_CLASSIFICATION_ATTRIBUTE_HANDLER_H

#include "scalar_attribute_handler.h"

// "classification": a ScalarAttributeHandler that additionally maintains a
// 256-bucket histogram_ (count per classification value).
class ClassificationAttributeHandler : public ScalarAttributeHandler {
public:
    void updateStats(Attribute &attr, const uint8_t *rowsBegin,
                      uint64_t numPoints, uint64_t rowStride) const override;
};

#endif //PCLITE_CLASSIFICATION_ATTRIBUTE_HANDLER_H
