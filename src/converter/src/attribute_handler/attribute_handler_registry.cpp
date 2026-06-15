//
// Created by cj on 2026-06-14.
//

#include "attribute_handler_registry.h"

#include "classification_attribute_handler.h"
#include "position_attribute_handler.h"
#include "rgb_attribute_handler.h"
#include "scalar_attribute_handler.h"

AttributeHandler *AttributeHandlerRegistry::get(const Attribute &attr) {
    static PositionAttributeHandler position;
    static RGBAttributeHandler rgb;
    static ClassificationAttributeHandler classification;
    static ScalarAttributeHandler scalar;

    if (attr.name_ == "position") return &position;
    if (attr.name_ == "rgb") return &rgb;
    if (attr.name_ == "classification") return &classification;
    return &scalar;
}
