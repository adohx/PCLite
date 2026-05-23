#include "attributes.h"
#include <stdexcept>

// Returns the per-point byte stride offset of the named attribute
// (sum of sizes of preceding attributes).
// For column-major node data: block_offset = getOffset(name) * numPoints.
int Attributes::getOffset(const std::string& name) const {
    int offset = 0;
    for (const auto& attr : attr_)  {
        if (attr.name_ == name) return offset;
        offset += attr.size_;
    }
    return -1;
}

Attribute Attributes::getAttribute(const std::string& name) {
    for (auto& attr : attr_)
        if (attr.name_ == name) return attr;
    throw std::runtime_error("Attribute not found: " + name);
}
