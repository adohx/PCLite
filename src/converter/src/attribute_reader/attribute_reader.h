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


class AttributeReader : public std::enable_shared_from_this<AttributeReader> {
protected:
    explicit AttributeReader(std::string path);
    virtual ~AttributeReader();
public:
    static std::shared_ptr<AttributeReader> createReader(const std::string& path);

    // Returns a reader usable concurrently with this one, for callers that
    // want to read disjoint ranges from multiple threads at once. Formats
    // that read via a single sequential stream/cursor (e.g. LASzip) must
    // override this to open an independent instance — mirrors
    // PotreeConverter's per-task laszip_POINTER instances (see
    // Converter/src/chunker_countsort_laszip.cpp). The default assumes
    // readPositions()/readRawData() are already safe to call concurrently on
    // the same instance (true for e.g. a plain in-memory reader) and just
    // hands back this same instance.
    virtual std::shared_ptr<AttributeReader> clone() const {
        return std::const_pointer_cast<AttributeReader>(shared_from_this());
    }

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

    // Returns [index, index+count) point rows, packed back-to-back according
    // to the layout described by getAttributes() (row stride = getAttributes().getTotalBytes()).
    virtual std::vector<uint8_t> readRawData(uint64_t index, int64_t count) = 0;

    virtual ReaderType getType() = 0;

public:
    Attributes& getAttributes();

protected:
    std::string path_;
    Attributes attributes_;
};


#endif //PCLITE_ATTRIBUTEREADER_H
