//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_ATTRIBUTES_H
#define PCLITE_ATTRIBUTES_H

#include <string>
#include <vector>
#include "vec3.h"

enum class AttributeType {
    INT8 = 0,
    INT16 = 1,
    INT32 = 2,
    INT64 = 3,

    UINT8 = 10,
    UINT16 = 11,
    UINT32 = 12,
    UINT64 = 13,

    FLOAT = 20,
    DOUBLE = 21,

    UNDEFINED = 123456,
};

struct Attribute {
    std::string name_;
    std::string description_;
    int size_;
    int numElements_;
    AttributeType type_;

    vec3d min_;
    vec3d max_;

    std::vector<int64_t> histogram_ =  std::vector<int64_t>(256, 0);

};

struct Attributes {

    std::vector<Attribute> attributes_;
    uint32_t bytes_;
    vec3d posScale_;
    vec3d posOffset_;


    int getOffset(const std::string& name) const;
    Attribute getAttribute(const std::string& name);
};

#endif //PCLITE_ATTRIBUTES_H
