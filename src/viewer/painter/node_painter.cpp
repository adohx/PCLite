#include "node_painter.h"
#include "../node_loader/node.h"
#include <SDL2/SDL_opengl.h>

NodePainter::NodePainter(Attributes attributes)
    : attributes_(std::move(attributes)) {}

NodePainter::Batch NodePainter::decode(const Node* node) const {
    Batch b;
    const auto& data = node->data;
    uint32_t N = node->numPoints_;
    if (N == 0) return b;

    // Column-major layout: block offset = getOffset(name) * N
    int posOff = attributes_.getOffset("position");
    if (posOff >= 0) {
        const auto* raw = reinterpret_cast<const int32_t*>(
            data.data() + (uint64_t)posOff * N);
        b.positions.reserve(N * 3);
        for (uint32_t i = 0; i < N; ++i) {
            // b.positions.push_back((float)(raw[i*3+0] * attributes_.posScale_.x
            //                               + attributes_.posOffset_.x));
            // b.positions.push_back((float)(raw[i*3+1] * attributes_.posScale_.y
            //                               + attributes_.posOffset_.y));
            // b.positions.push_back((float)(raw[i*3+2] * attributes_.posScale_.z
            //                               + attributes_.posOffset_.z));
        }
    }

    int rgbOff = attributes_.getOffset("rgb");
    if (rgbOff >= 0) {
        const auto* raw = reinterpret_cast<const uint16_t*>(
            data.data() + (uint64_t)rgbOff * N);
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

void NodePainter::paint(const Mat4f&) {
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
