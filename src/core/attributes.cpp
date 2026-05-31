#include "attributes.h"
#include <stdexcept>

bool Attributes::pushAttribute(const Attribute &attr) {
    if (nameToAttrIdx_.contains(attr.name_))
        return false;

    nameToOffset_.insert(std::make_pair(attr.name_, totalBytes_));
    nameToAttrIdx_.insert(std::make_pair(attr.name_, static_cast<int>(size())));

    emplace_back(attr);
    totalBytes_ += attr.bytes_;
    return true;
}

// Returns the per-point byte stride offset of the named attribute
// (sum of sizes of preceding attributes).
// For column-major node data: block_offset = getOffset(name) * numPoints.
uint64_t Attributes::getOffset(const std::string &name) const {
    auto ite=nameToOffset_.find(name);
    if (ite!=nameToOffset_.end())
        return ite->second;
    return -1;
}

Attribute Attributes::getAttribute(const std::string &name) {
    if (!nameToAttrIdx_.contains(name))
        return Attribute();
    return at(nameToAttrIdx_[name]);
}

uint64_t Attributes::getTotalBytes() const {
    return totalBytes_;
}
