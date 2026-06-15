//
// Created by cj on 2026-06-14.
//

#ifndef PCLITE_ATTRIBUTE_HANDLER_H
#define PCLITE_ATTRIBUTE_HANDLER_H

#include <cstdint>
#include <vector>

#include "attributes.h"

// Per-attribute encode/decode/stats/merge algorithm. Implementations are
// stateless (all methods const) and shared as singletons via
// AttributeHandlerRegistry.
//
// Unless noted otherwise, `dst`/`src`/`dstRow`/`srcRows` point at the start of
// *this attribute's* bytes within a point row (i.e. row pointer + attributes.getOffset(name)),
// not at the start of the whole row.
class AttributeHandler {
public:
    virtual ~AttributeHandler() = default;

    // Reads one row's worth of srcAttr from `src` (source row layout), re-quantizes
    // it according to dstAttr's scale_/offset_/type_, and writes it to `dst`
    // (destination row layout).
    virtual void encode(uint8_t *dst, const uint8_t *src,
                         const Attribute &srcAttr, const Attribute &dstAttr) const = 0;

    // Decodes the binary value at `src` (per attr's type_/scale_/offset_) into
    // out[0..attr.numElements_) as world-space doubles.
    virtual void decode(const uint8_t *src, const Attribute &attr, double *out) const = 0;

    // Incrementally folds the rows in [rowsBegin, rowsBegin + numPoints*rowStride)
    // into attr's min_/max_ (and histogram_ for classification-like attributes).
    //
    // Precondition: attr.min_/attr.max_ must already be initialized to sentinel
    // values (e.g. +inf / -inf) by the caller before the first call, since this
    // method only ever widens the range.
    virtual void updateStats(Attribute &attr, const uint8_t *rowsBegin,
                              uint64_t numPoints, uint64_t rowStride) const = 0;

    // Chooses/combines this attribute's value for dstRow from the candidate rows.
    virtual void merge(uint8_t *dstRow, const std::vector<const uint8_t *> &srcRows,
                        size_t selectedIndex, const Attribute &attr) const = 0;
};

#endif //PCLITE_ATTRIBUTE_HANDLER_H
