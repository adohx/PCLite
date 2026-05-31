//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_ATTRIBUTES_H
#define PCLITE_ATTRIBUTES_H

#include <string>
#include <unordered_map>
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
    int bytes_;
    int numElements_;
    AttributeType type_;

    vec3d min_;
    vec3d max_;

    vec3d scale_;
    vec3d offset_;
    
    std::vector<int64_t> histogram_ =  std::vector<int64_t>(256, 0);

};

struct Attributes:public std::vector<Attribute> {

    Attributes() = default;

    bool pushAttribute(const Attribute& attr);
    uint64_t getOffset(const std::string& name) const;
    Attribute getAttribute(const std::string& name);
    uint64_t getTotalBytes() const;
private:
    uint64_t totalBytes_ = 0;
    std::unordered_map<std::string, uint64_t> nameToOffset_;
    std::unordered_map<std::string, int> nameToAttrIdx_;
};

#endif //PCLITE_ATTRIBUTES_H
