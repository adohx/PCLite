#include "bounding_box_painter.h"
#include "../node_loader/node.h"
#include <SDL2/SDL_opengl.h>
#include <limits>
#include <algorithm>

void BoundingBoxPainter::addNode(Node* node) {
    nodeBBoxes_[node] = std::make_pair(node->bb_.min(), node->bb_.max());
    dirty_ = true;
}

void BoundingBoxPainter::removeNode(Node* node) {
    nodeBBoxes_.erase(node);
    dirty_ = true;
}

void BoundingBoxPainter::recompute() {
    constexpr double inf = std::numeric_limits<double>::infinity();
    overallMin_ = { inf,  inf,  inf};
    overallMax_ = {-inf, -inf, -inf};

    for (auto& [node, bb] : nodeBBoxes_) {
        auto& [mn, mx] = bb;
        overallMin_.x = std::min(overallMin_.x, mn.x);
        overallMin_.y = std::min(overallMin_.y, mn.y);
        overallMin_.z = std::min(overallMin_.z, mn.z);
        overallMax_.x = std::max(overallMax_.x, mx.x);
        overallMax_.y = std::max(overallMax_.y, mx.y);
        overallMax_.z = std::max(overallMax_.z, mx.z);
    }
    dirty_ = false;
}

void BoundingBoxPainter::paint(const Mat4f&) {
    if (nodeBBoxes_.empty()) return;
    if (dirty_) recompute();

    float x0 = (float)overallMin_.x, y0 = (float)overallMin_.y, z0 = (float)overallMin_.z;
    float x1 = (float)overallMax_.x, y1 = (float)overallMax_.y, z1 = (float)overallMax_.z;

    // 8 corners, 12 edges
    //   4----5
    //  /|   /|
    // 7----6 |
    // | 0--|-1
    // |/   |/
    // 3----2
    float vx[8] = {x0,x1,x1,x0, x0,x1,x1,x0};
    float vy[8] = {y0,y0,y0,y0, y1,y1,y1,y1};
    float vz[8] = {z0,z0,z1,z1, z0,z0,z1,z1};

    // Edge index pairs
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // bottom face
        {4,5},{5,6},{6,7},{7,4},  // top face
        {0,4},{1,5},{2,6},{3,7},  // vertical
    };

    glLineWidth(1.5f);
    glColor3f(1.f, 1.f, 0.f);   // yellow
    glBegin(GL_LINES);
    for (auto& [a, b] : edges) {
        glVertex3f(vx[a], vy[a], vz[a]);
        glVertex3f(vx[b], vy[b], vz[b]);
    }
    glEnd();
}
