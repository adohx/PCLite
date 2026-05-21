#include "node_painter.h"
#include "node.h"
#include <SDL2/SDL_opengl.h>

NodePainter::NodePainter(PointLayout layout) : layout_(std::move(layout)) {}

NodePainter::Batch NodePainter::decode(const Node* node) const {
    Batch b;
    const auto& data = node->data();
    uint32_t N = node->pointCount();
    if (N == 0) return b;

    // Column-major storage: all N values of attr[0], then all N of attr[1], ...
    std::vector<uint64_t> attrOffset(layout_.attrSizes.size());
    uint64_t cursor = 0;
    for (size_t i = 0; i < layout_.attrSizes.size(); ++i) {
        attrOffset[i] = cursor;
        cursor += (uint64_t)N * layout_.attrSizes[i];
    }

    // Position: int32[3] per point, world = raw * scale + offset
    if (layout_.posIdx >= 0 && (size_t)layout_.posIdx < attrOffset.size()) {
        const auto* raw = reinterpret_cast<const int32_t*>(data.data() + attrOffset[layout_.posIdx]);
        b.positions.reserve(N * 3);
        for (uint32_t i = 0; i < N; ++i) {
            b.positions.push_back((float)(raw[i*3+0] * layout_.scale.x + layout_.offset.x));
            b.positions.push_back((float)(raw[i*3+1] * layout_.scale.y + layout_.offset.y));
            b.positions.push_back((float)(raw[i*3+2] * layout_.scale.z + layout_.offset.z));
        }
    }

    // RGB: uint16[3] per point, normalized to [0, 1]
    if (layout_.rgbIdx >= 0 && (size_t)layout_.rgbIdx < attrOffset.size()) {
        const auto* raw = reinterpret_cast<const uint16_t*>(data.data() + attrOffset[layout_.rgbIdx]);
        b.colors.reserve(N * 3);
        for (uint32_t i = 0; i < N; ++i) {
            b.colors.push_back(raw[i*3+0] / 65535.f);
            b.colors.push_back(raw[i*3+1] / 65535.f);
            b.colors.push_back(raw[i*3+2] / 65535.f);
        }
    } else {
        b.colors.assign(N * 3, 0.8f);
    }

    return b;
}

void NodePainter::addNode(Node* node) {
    batches_[node] = decode(node);
}

void NodePainter::removeNode(Node* node) {
    batches_.erase(node);
}

void NodePainter::paint() {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    for (auto& [node, batch] : batches_) {
        if (batch.positions.empty()) continue;
        glVertexPointer(3, GL_FLOAT, 0, batch.positions.data());
        glColorPointer(3,  GL_FLOAT, 0, batch.colors.data());
        glDrawArrays(GL_POINTS, 0, (GLsizei)(batch.positions.size() / 3));
    }

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}
