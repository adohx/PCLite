#include "bounding_box_painter.h"
#include "../../core/node.h"
#include <glad/gl.h>
#include <limits>
#include <algorithm>
#include <vector>

BoundingBoxPainter::BoundingBoxPainter()
    : shader_(kBasicColorVertexSrc, kBasicColorFragmentSrc) {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

BoundingBoxPainter::~BoundingBoxPainter() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void BoundingBoxPainter::addNode(Node* node) {
    nodeBBoxes_[node] = std::make_pair(node->tightBB_.min(), node->tightBB_.max());
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

    if (nodeBBoxes_.empty()) {
        lineVertexCount_ = 0;
        return;
    }

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

    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // bottom face
        {4,5},{5,6},{6,7},{7,4},  // top face
        {0,4},{1,5},{2,6},{3,7},  // vertical
    };

    std::vector<float> verts;
    verts.reserve(12 * 2 * 6);
    constexpr float r = 1.f, g = 1.f, bcol = 0.f; // yellow
    for (auto& [a, b] : edges) {
        verts.insert(verts.end(), {vx[a], vy[a], vz[a], r, g, bcol});
        verts.insert(verts.end(), {vx[b], vy[b], vz[b], r, g, bcol});
    }

    lineVertexCount_ = (int)(verts.size() / 6);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
}

void BoundingBoxPainter::paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    if (nodeBBoxes_.empty()) return;
    if (dirty_) recompute();
    if (lineVertexCount_ == 0) return;

    shader_.use();
    shader_.setMat4("uMVP", projMatrix * viewMatrix);

    glLineWidth(1.5f);
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, lineVertexCount_);
    glBindVertexArray(0);
}
