#ifndef PCLITE_POINT_LAYOUT_H
#define PCLITE_POINT_LAYOUT_H

#include <vector>
#include <cstdint>
#include "vec3.h"

struct PointLayout {
    std::vector<uint32_t> attrSizes;  // byte size per point for each attribute
    int    posIdx = -1;               // index into attrSizes for position
    int    rgbIdx = -1;               // index into attrSizes for rgb
    vec3d  scale  = {1.0, 1.0, 1.0};
    vec3d  offset = {0.0, 0.0, 0.0};
};

#endif //PCLITE_POINT_LAYOUT_H
