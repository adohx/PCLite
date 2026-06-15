//
// Created by cj on 2026-05-13.
//

#include "las_reader.h"

#include <cstring>
#include <stdexcept>

#include <laszip_api.h>

namespace {

[[noreturn]] void throwLaszipError(laszip_POINTER ptr, const std::string &what) {
    laszip_CHAR *msg = nullptr;
    laszip_get_error(ptr, &msg);
    throw std::runtime_error("LasReader: " + what + ": " + (msg ? msg : "unknown error"));
}

} // namespace

LasReader::LasReader(const std::string &path) : AttributeReader(path) {
    laszip_POINTER reader = nullptr;
    if (laszip_create(&reader)) {
        throw std::runtime_error("LasReader: laszip_create failed");
    }
    reader_ = reader;

    laszip_BOOL isCompressed = 0;
    if (laszip_open_reader(reader_, path.c_str(), &isCompressed)) {
        throwLaszipError(reader_, "failed to open '" + path + "'");
    }

    laszip_header_struct *header = nullptr;
    if (laszip_get_header_pointer(reader_, &header)) {
        throwLaszipError(reader_, "laszip_get_header_pointer failed");
    }
    header_ = header;

    laszip_point_struct *point = nullptr;
    if (laszip_get_point_pointer(reader_, &point)) {
        throwLaszipError(reader_, "laszip_get_point_pointer failed");
    }
    point_ = point;

    pointFormat_ = header->point_data_format;
    if (pointFormat_ < 0 || pointFormat_ > 3) {
        throw std::runtime_error("LasReader: point data format " + std::to_string(pointFormat_) +
                                  " is not supported yet (only formats 0-3 are implemented)");
    }
    hasRgb_ = (pointFormat_ == 2 || pointFormat_ == 3);

    scale_ = {header->x_scale_factor, header->y_scale_factor, header->z_scale_factor};
    offset_ = {header->x_offset, header->y_offset, header->z_offset};

    numPoints_ = header->number_of_point_records;
    extendedNumPoints_ = header->extended_number_of_point_records;

    buildAttributes();
}

LasReader::~LasReader() {
    if (reader_) {
        laszip_close_reader(reader_);
        laszip_destroy(reader_);
    }
}

void LasReader::buildAttributes() {
    auto *header = static_cast<laszip_header_struct *>(header_);

    Attribute position;
    position.name_ = "position";
    position.description_ = "";
    position.numElements_ = 3;
    position.type_ = AttributeType::INT32;
    position.bytes_ = 12;
    position.scale_ = scale_;
    position.offset_ = offset_;
    position.min_ = {header->min_x, header->min_y, header->min_z};
    position.max_ = {header->max_x, header->max_y, header->max_z};
    attributes_.pushAttribute(position);
    offPosition_ = attributes_.getOffset("position");

    auto pushScalar = [&](const std::string &name, AttributeType type, int bytes, uint64_t &offOut) {
        Attribute attr;
        attr.name_ = name;
        attr.description_ = "";
        attr.numElements_ = 1;
        attr.type_ = type;
        attr.bytes_ = bytes;
        attr.scale_ = {1, 1, 1};
        attr.offset_ = {0, 0, 0};
        attributes_.pushAttribute(attr);
        offOut = attributes_.getOffset(name);
    };

    pushScalar("intensity", AttributeType::UINT16, 2, offIntensity_);
    pushScalar("return number", AttributeType::UINT8, 1, offReturnNumber_);
    pushScalar("number of returns", AttributeType::UINT8, 1, offNumberOfReturns_);
    pushScalar("classification", AttributeType::UINT8, 1, offClassification_);
    pushScalar("scan angle rank", AttributeType::UINT8, 1, offScanAngleRank_);
    pushScalar("user data", AttributeType::UINT8, 1, offUserData_);
    pushScalar("point source id", AttributeType::UINT16, 2, offPointSourceId_);

    if (hasRgb_) {
        Attribute rgb;
        rgb.name_ = "rgb";
        rgb.description_ = "";
        rgb.numElements_ = 3;
        rgb.type_ = AttributeType::UINT16;
        rgb.bytes_ = 6;
        rgb.scale_ = {1, 1, 1};
        rgb.offset_ = {0, 0, 0};
        attributes_.pushAttribute(rgb);
        offRgb_ = attributes_.getOffset("rgb");
    }

    rowBytes_ = attributes_.getTotalBytes();
}

AttributeReader::HeaderInfo LasReader::headerInfo() {
    auto *header = static_cast<laszip_header_struct *>(header_);

    HeaderInfo info;
    info.name_ = path_;
    info.description_ = "";
    info.numPoints_ = numPoints_;
    info.extendedNumPoints_ = extendedNumPoints_;
    info.min_ = {header->min_x, header->min_y, header->min_z};
    info.max_ = {header->max_x, header->max_y, header->max_z};
    info.bpp_ = static_cast<int>(rowBytes_);
    return info;
}

vec3d LasReader::decodeCurrentPosition() const {
    auto *point = static_cast<laszip_point_struct *>(point_);
    return {
        point->X * scale_.x + offset_.x,
        point->Y * scale_.y + offset_.y,
        point->Z * scale_.z + offset_.z,
    };
}

vec3d LasReader::readPosition(uint64_t index) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (laszip_seek_point(reader_, static_cast<laszip_I64>(index))) {
        throwLaszipError(reader_, "laszip_seek_point failed");
    }
    if (laszip_read_point(reader_)) {
        throwLaszipError(reader_, "laszip_read_point failed");
    }
    return decodeCurrentPosition();
}

std::vector<vec3d> LasReader::readPositions(uint64_t index, int64_t counts) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<vec3d> result;
    if (counts <= 0) return result;
    result.reserve(static_cast<size_t>(counts));

    if (laszip_seek_point(reader_, static_cast<laszip_I64>(index))) {
        throwLaszipError(reader_, "laszip_seek_point failed");
    }

    for (int64_t i = 0; i < counts; ++i) {
        if (laszip_read_point(reader_)) {
            throwLaszipError(reader_, "laszip_read_point failed");
        }
        result.push_back(decodeCurrentPosition());
    }
    return result;
}

std::vector<uint8_t> LasReader::readRawData(uint64_t index, int64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count <= 0) return {};

    std::vector<uint8_t> out(rowBytes_ * static_cast<uint64_t>(count));

    if (laszip_seek_point(reader_, static_cast<laszip_I64>(index))) {
        throwLaszipError(reader_, "laszip_seek_point failed");
    }

    auto *point = static_cast<laszip_point_struct *>(point_);
    for (int64_t i = 0; i < count; ++i) {
        if (laszip_read_point(reader_)) {
            throwLaszipError(reader_, "laszip_read_point failed");
        }

        uint8_t *row = out.data() + static_cast<uint64_t>(i) * rowBytes_;

        int32_t x = point->X, y = point->Y, z = point->Z;
        std::memcpy(row + offPosition_ + 0, &x, 4);
        std::memcpy(row + offPosition_ + 4, &y, 4);
        std::memcpy(row + offPosition_ + 8, &z, 4);

        uint16_t intensity = point->intensity;
        std::memcpy(row + offIntensity_, &intensity, 2);

        row[offReturnNumber_] = static_cast<uint8_t>(point->return_number);
        row[offNumberOfReturns_] = static_cast<uint8_t>(point->number_of_returns);
        row[offClassification_] = static_cast<uint8_t>(point->classification);
        row[offScanAngleRank_] = static_cast<uint8_t>(point->scan_angle_rank);
        row[offUserData_] = point->user_data;

        uint16_t pointSourceId = point->point_source_ID;
        std::memcpy(row + offPointSourceId_, &pointSourceId, 2);

        if (hasRgb_) {
            uint16_t r = point->rgb[0], g = point->rgb[1], b = point->rgb[2];
            std::memcpy(row + offRgb_ + 0, &r, 2);
            std::memcpy(row + offRgb_ + 2, &g, 2);
            std::memcpy(row + offRgb_ + 4, &b, 2);
        }
    }
    return out;
}
