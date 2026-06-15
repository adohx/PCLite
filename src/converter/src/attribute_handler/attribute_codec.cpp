//
// Created by cj on 2026-06-14.
//

#include "attribute_codec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace attribute_codec {

namespace {

template <typename T>
double readAs(const uint8_t *p) {
    T v;
    std::memcpy(&v, p, sizeof(T));
    return static_cast<double>(v);
}

template <typename T>
void writeAs(uint8_t *p, double value) {
    T v;
    if constexpr (std::is_integral_v<T>) {
        double rounded = std::round(value);
        rounded = std::clamp(rounded,
                              static_cast<double>(std::numeric_limits<T>::lowest()),
                              static_cast<double>(std::numeric_limits<T>::max()));
        v = static_cast<T>(rounded);
    } else {
        v = static_cast<T>(value);
    }
    std::memcpy(p, &v, sizeof(T));
}

} // namespace

int elementBytes(AttributeType type) {
    switch (type) {
        case AttributeType::INT8:
        case AttributeType::UINT8:
            return 1;
        case AttributeType::INT16:
        case AttributeType::UINT16:
            return 2;
        case AttributeType::INT32:
        case AttributeType::UINT32:
        case AttributeType::FLOAT:
            return 4;
        case AttributeType::INT64:
        case AttributeType::UINT64:
        case AttributeType::DOUBLE:
            return 8;
        default:
            return 0;
    }
}

double readElement(const uint8_t *p, AttributeType type) {
    switch (type) {
        case AttributeType::INT8:   return readAs<int8_t>(p);
        case AttributeType::INT16:  return readAs<int16_t>(p);
        case AttributeType::INT32:  return readAs<int32_t>(p);
        case AttributeType::INT64:  return readAs<int64_t>(p);
        case AttributeType::UINT8:  return readAs<uint8_t>(p);
        case AttributeType::UINT16: return readAs<uint16_t>(p);
        case AttributeType::UINT32: return readAs<uint32_t>(p);
        case AttributeType::UINT64: return readAs<uint64_t>(p);
        case AttributeType::FLOAT:  return readAs<float>(p);
        case AttributeType::DOUBLE: return readAs<double>(p);
        default: return 0.0;
    }
}

void writeElement(uint8_t *p, AttributeType type, double value) {
    switch (type) {
        case AttributeType::INT8:   writeAs<int8_t>(p, value); return;
        case AttributeType::INT16:  writeAs<int16_t>(p, value); return;
        case AttributeType::INT32:  writeAs<int32_t>(p, value); return;
        case AttributeType::INT64:  writeAs<int64_t>(p, value); return;
        case AttributeType::UINT8:  writeAs<uint8_t>(p, value); return;
        case AttributeType::UINT16: writeAs<uint16_t>(p, value); return;
        case AttributeType::UINT32: writeAs<uint32_t>(p, value); return;
        case AttributeType::UINT64: writeAs<uint64_t>(p, value); return;
        case AttributeType::FLOAT:  writeAs<float>(p, value); return;
        case AttributeType::DOUBLE: writeAs<double>(p, value); return;
        default: return;
    }
}

double component(const vec3d &v, int i) {
    switch (i) {
        case 0: return v.x;
        case 1: return v.y;
        default: return v.z;
    }
}

void setComponent(vec3d &v, int i, double value) {
    switch (i) {
        case 0: v.x = value; return;
        case 1: v.y = value; return;
        default: v.z = value; return;
    }
}

void encodeGeneric(uint8_t *dst, const uint8_t *src,
                   const Attribute &srcAttr, const Attribute &dstAttr) {
    int srcElemBytes = srcAttr.numElements_ > 0 ? srcAttr.bytes_ / srcAttr.numElements_ : 0;
    int dstElemBytes = dstAttr.numElements_ > 0 ? dstAttr.bytes_ / dstAttr.numElements_ : 0;

    for (int i = 0; i < dstAttr.numElements_; ++i) {
        double raw = (i < srcAttr.numElements_) ? readElement(src + i * srcElemBytes, srcAttr.type_) : 0.0;
        double world = raw * component(srcAttr.scale_, i) + component(srcAttr.offset_, i);

        double scale = component(dstAttr.scale_, i);
        double quantized = (scale != 0.0) ? (world - component(dstAttr.offset_, i)) / scale
                                           : world - component(dstAttr.offset_, i);

        writeElement(dst + i * dstElemBytes, dstAttr.type_, quantized);
    }
}

void decodeGeneric(const uint8_t *src, const Attribute &attr, double *out) {
    int elemBytes = attr.numElements_ > 0 ? attr.bytes_ / attr.numElements_ : 0;
    for (int i = 0; i < attr.numElements_; ++i) {
        double raw = readElement(src + i * elemBytes, attr.type_);
        out[i] = raw * component(attr.scale_, i) + component(attr.offset_, i);
    }
}

void updateRangeStats(Attribute &attr, const uint8_t *rowsBegin,
                       uint64_t numPoints, uint64_t rowStride) {
    double decoded[3] = {0.0, 0.0, 0.0};

    for (uint64_t i = 0; i < numPoints; ++i) {
        const uint8_t *row = rowsBegin + i * rowStride;
        decodeGeneric(row, attr, decoded);

        for (int c = 0; c < attr.numElements_; ++c) {
            double v = decoded[c];
            if (v < component(attr.min_, c)) setComponent(attr.min_, c, v);
            if (v > component(attr.max_, c)) setComponent(attr.max_, c, v);
        }
    }
}

void mergeCopySelected(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                        size_t selectedIndex, const Attribute &attr) {
    std::memcpy(dstRow, srcRows[selectedIndex], attr.bytes_);
}

} // namespace attribute_codec
