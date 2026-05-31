#include "node.h"

#include <cstring>

// ── setData ──────────────────────────────────────────────────────────────────
// The on-disk layout is column-major (attribute-separated):
//   [attr_A for all N points][attr_B for all N points]...
// block start for attribute X = getOffset(X) * N  (bytes)

bool Node::setData(const std::vector<uint8_t>& data, const Attributes& attributes) {
    if (data.empty()) return false;
    attributes_ = attributes;

    uint64_t N = data.size() / attributes_.getTotalBytes();
    if (N == 0) return false;

    Attribute posAttr = attributes_.getAttribute("position");
    auto posOffset = attributes_.getOffset("position");
    auto totalBytes = attributes_.getTotalBytes();


    if (!posAttr.name_.empty() && posAttr.numElements_ >= 3) {
        points_.clear();
        points_.reserve(N);

        for (auto i=0;i<N;++i) {
            auto x=readData<int>(data.data(),i*totalBytes+posOffset+0)*posAttr.scale_.x + posAttr.offset_.x;
            auto y=readData<int>(data.data(),i*totalBytes+posOffset+4)*posAttr.scale_.y + posAttr.offset_.y;
            auto z=readData<int>(data.data(),i*totalBytes+posOffset+8)*posAttr.scale_.z + posAttr.offset_.z;
            tightBB_.expand({x, y, z});
            points_.push_back(std::move(vec3f(x,y,z)));
        }
    }

    Attribute rgbAddr = attributes_.getAttribute("rgb");
    auto rgbOffset = attributes_.getOffset("rgb");
    if (!rgbAddr.name_.empty() && rgbAddr.numElements_ >= 3) {
        colors_.clear();
        colors_.reserve(N);

        for (auto i=0;i<N;++i) {
            auto r=readData<uint16_t>(data.data(),i*totalBytes+rgbOffset+0)*rgbAddr.scale_.x + rgbAddr.offset_.x;
            auto g=readData<uint16_t>(data.data(),i*totalBytes+rgbOffset+2)*rgbAddr.scale_.y + rgbAddr.offset_.y;
            auto b=readData<uint16_t>(data.data(),i*totalBytes+rgbOffset+4)*rgbAddr.scale_.z + rgbAddr.offset_.z;
            colors_.push_back(std::move(vec3f(r,g,b)));
        }
    }

    return true;
}

// ── getPoints / getColors ─────────────────────────────────────────────────────

std::vector<vec3f> Node::getPoints() const { return points_; }
std::vector<vec3f> Node::getColors() const { return colors_; }

// ── Private helpers ───────────────────────────────────────────────────────────

std::vector<vec3f> Node::decodeBlock(const std::vector<uint8_t>& data,
                                     uint64_t N,
                                     const Attribute& attr,
                                     uint64_t blockOff) const {
    int elemBytes = attr.bytes_ / attr.numElements_;
    std::vector<vec3f> result;
    result.reserve(N);

    for (uint64_t i = 0; i < N; ++i) {
        auto readElem = [&](int elem) -> double {
            uint64_t off = blockOff + i * (uint64_t)attr.bytes_ + elem * elemBytes;
            switch (attr.type_) {
                case AttributeType::INT8:   { int8_t   v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::INT16:  { int16_t  v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::INT32:  { int32_t  v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::INT64:  { int64_t  v; memcpy(&v, data.data()+off, elemBytes); return static_cast<double>(v); }
                case AttributeType::UINT8:  { uint8_t  v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::UINT16: { uint16_t v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::UINT32: { uint32_t v; memcpy(&v, data.data()+off, elemBytes); return static_cast<double>(v); }
                case AttributeType::UINT64: { uint64_t v; memcpy(&v, data.data()+off, elemBytes); return static_cast<double>(v); }
                case AttributeType::FLOAT:  { float    v; memcpy(&v, data.data()+off, elemBytes); return v; }
                case AttributeType::DOUBLE: { double   v; memcpy(&v, data.data()+off, elemBytes); return v; }
                default: return 0.0;
            }
        };

        vec3f p;
        p.x = static_cast<float>(readElem(0) * attr.scale_.x + attr.offset_.x);
        p.y = static_cast<float>(readElem(1) * attr.scale_.y + attr.offset_.y);
        p.z = static_cast<float>(readElem(2) * attr.scale_.z + attr.offset_.z);
        result.push_back(p);
    }
    return result;
}
