//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_ATTRIBUTE_HANDLER_REGISTRY_H
#define PCLITE_ATTRIBUTE_HANDLER_REGISTRY_H

#include "attribute_handler.h"

// Maps an Attribute (by name_/type_) to its singleton AttributeHandler.
class AttributeHandlerRegistry {
public:
    static AttributeHandler *get(const Attribute &attr);
};

#endif //PCLITE_ATTRIBUTE_HANDLER_REGISTRY_H
