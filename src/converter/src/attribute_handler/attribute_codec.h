//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_ATTRIBUTE_CODEC_H
#define PCLITE_ATTRIBUTE_CODEC_H

#include <cstdint>
#include <vector>

#include "attributes.h"

// Shared, type-erased read/write/encode/decode/stats/merge helpers used by the
// concrete AttributeHandler implementations. All functions operate on a
// pointer to the start of a single attribute's bytes (not the whole row).
namespace attribute_codec {

// Size in bytes of a single element of `type`.
int elementBytes(AttributeType type);

// Reads one element of `type` at `p` and returns it as a raw (unscaled) double.
double readElement(const uint8_t *p, AttributeType type);

// Rounds (for integer types) and clamps `value` to `type`'s range, then writes
// it at `p`.
void writeElement(uint8_t *p, AttributeType type, double value);

// Component i (0=x, 1=y, 2=z) of a vec3d.
double component(const vec3d &v, int i);
void setComponent(vec3d &v, int i, double value);

// Generic encode: decode srcAttr's element i to world space using srcAttr's
// scale_/offset_, then re-quantize into dstAttr's type_/scale_/offset_.
void encodeGeneric(uint8_t *dst, const uint8_t *src,
                    const Attribute &srcAttr, const Attribute &dstAttr);

// Generic decode: out[i] = raw_i * attr.scale_[i] + attr.offset_[i] for
// i in [0, attr.numElements_).
void decodeGeneric(const uint8_t *src, const Attribute &attr, double *out);

// Generic stats: widen attr.min_/max_ component-wise (for i in
// [0, attr.numElements_)) over the decoded values of every row.
void updateRangeStats(Attribute &attr, const uint8_t *rowsBegin,
                       uint64_t numPoints, uint64_t rowStride);

// Generic merge: copy attr.bytes_ bytes from srcRows[selectedIndex] to dstRow.
void mergeCopySelected(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                        size_t selectedIndex, const Attribute &attr);

} // namespace attribute_codec

#endif //PCLITE_ATTRIBUTE_CODEC_H
