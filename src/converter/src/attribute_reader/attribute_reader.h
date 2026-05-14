//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_ATTRIBUTEREADER_H
#define PCLITE_ATTRIBUTEREADER_H
#include <memory>
#include <string>
#include <vector>

#include "attributes.h"
#include "vec3.h"


class AttributeReader {
protected:
    explicit AttributeReader(std::string path);
    virtual ~AttributeReader();
public:
    static std::shared_ptr<AttributeReader> createReader(const std::string& path);
    enum ReaderType {
        LAS,
        PCD
    };


    struct HeaderInfo{
        std::string name_;
        std::string description_;
        uint64_t numPoints_;
        uint64_t extendedNumPoints_;
        vec3d min_;
        vec3d max_;
        int bpp_;
    };

    virtual HeaderInfo headerInfo() = 0;
    virtual vec3d readPosition(uint64_t index) = 0;
    virtual std::vector<vec3d> readPositions(uint64_t index,int64_t counts) = 0;
    virtual ReaderType getType() = 0;

public:
    Attributes& getAttributes();

protected:
    std::string path_;
    Attributes attributes_;
};


#endif //PCLITE_ATTRIBUTEREADER_H
