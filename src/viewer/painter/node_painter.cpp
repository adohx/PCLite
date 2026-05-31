#include "node_painter.h"
#include "../../core/node.h"
#include <SDL2/SDL_opengl.h>

NodePainter::NodePainter(Attributes attributes)
    : attributes_(std::move(attributes)) {}

NodePainter::Batch NodePainter::decode(const Node* node) const {
    Batch b;
    uint32_t N = node->numPoints_;
    if (N == 0) return b;

    auto points = node->getPoints();
    b.positions.reserve(points.size() * 3);
    for (const auto& p : points) {
        b.positions.push_back(p.x);
        b.positions.push_back(p.y);
        b.positions.push_back(p.z);
    }

    auto colors = node->getColors();
    if (!colors.empty()) {
        b.colors.reserve(colors.size() * 3);
        for (const auto& c : colors) {
            b.colors.push_back(c.x / 65535.f);
            b.colors.push_back(c.y / 65535.f);
            b.colors.push_back(c.z / 65535.f);
        }
    } else {
        b.colors.assign(b.positions.size(), 0.8f);
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
