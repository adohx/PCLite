//
// Created by cj on 2026-05-13.
//

#ifndef PCLITE_LASREADER_H
#define PCLITE_LASREADER_H

#include <mutex>

#include "attribute_reader.h"

// Reads LAS/LAZ point clouds via LASzip (linked directly against the `laszip`
// C++ library, using the C API declared in laszip_api.h).
class LasReader : public AttributeReader {
public:
    explicit LasReader(const std::string &path);
    ~LasReader() override;

    HeaderInfo headerInfo() override;
    vec3d readPosition(uint64_t index) override;
    std::vector<vec3d> readPositions(uint64_t index, int64_t counts) override;
    std::vector<uint8_t> readRawData(uint64_t index, int64_t count) override;
    ReaderType getType() override { return LAS; }

    // LASzip reads via one sequential seek/read cursor per laszip_POINTER,
    // serialized by mutex_ below — concurrent callers need their own
    // instance each, so reopen the same file instead of sharing this one.
    std::shared_ptr<AttributeReader> clone() const override { return createReader(path_); }

private:
    void buildAttributes();
    vec3d decodeCurrentPosition() const;

private:
    // Opaque laszip handles (laszip_POINTER / laszip_header_struct* / laszip_point_struct*).
    void *reader_ = nullptr;
    void *header_ = nullptr;
    void *point_ = nullptr;

    int pointFormat_ = 0;
    bool hasRgb_ = false;
    vec3d scale_{1, 1, 1};
    vec3d offset_{0, 0, 0};

    // Byte offsets of each attribute within a readRawData() row.
    uint64_t offPosition_ = 0;
    uint64_t offIntensity_ = 0;
    uint64_t offReturnNumber_ = 0;
    uint64_t offNumberOfReturns_ = 0;
    uint64_t offClassification_ = 0;
    uint64_t offScanAngleRank_ = 0;
    uint64_t offUserData_ = 0;
    uint64_t offPointSourceId_ = 0;
    uint64_t offRgb_ = 0;
    uint64_t rowBytes_ = 0;

    uint64_t numPoints_ = 0;
    uint64_t extendedNumPoints_ = 0;

    // laszip reads are sequential (seek + read); serialize concurrent access.
    mutable std::mutex mutex_;
};


#endif //PCLITE_LASREADER_H
