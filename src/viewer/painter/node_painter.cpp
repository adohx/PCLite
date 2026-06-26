#include "node_painter.h"
#include "../../core/node.h"
#include <glad/gl.h>
#include <vector>
#include <cstdint>

NodePainter::NodePainter(Attributes attributes)
    : attributes_(std::move(attributes)),
      shader_(kPointCloudVertexSrc, kPointCloudFragmentSrc),
      pickShader_(kPickVertexSrc, kPickFragmentSrc) {}

NodePainter::~NodePainter() {
    for (auto& [node, batch] : batches_) destroyBatch(batch);
}

NodePainter::Batch NodePainter::createBatch(const Node* node) const {
    Batch b;
    uint32_t N = node->numPoints_;
    if (N == 0) return b;

    auto points = node->getPoints();
    std::vector<float> positions;
    positions.reserve(points.size() * 3);
    for (const auto& p : points) {
        positions.push_back(p.x);
        positions.push_back(p.y);
        positions.push_back(p.z);
    }

    std::vector<float> colors;
    auto nodeColors = node->getColors();
    if (!nodeColors.empty()) {
        colors.reserve(nodeColors.size() * 3);
        for (const auto& c : nodeColors) {
            colors.push_back(c.x / 65535.f);
            colors.push_back(c.y / 65535.f);
            colors.push_back(c.z / 65535.f);
        }
    } else {
        colors.assign(positions.size(), 0.8f);
    }

    b.count = (int)(positions.size() / 3);

    glGenVertexArrays(1, &b.vao);
    glBindVertexArray(b.vao);

    glGenBuffers(1, &b.vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, b.vboPos);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &b.vboColor);
    glBindBuffer(GL_ARRAY_BUFFER, b.vboColor);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return b;
}

void NodePainter::destroyBatch(Batch& batch) {
    if (batch.vboPos)   glDeleteBuffers(1, &batch.vboPos);
    if (batch.vboColor) glDeleteBuffers(1, &batch.vboColor);
    if (batch.vao)      glDeleteVertexArrays(1, &batch.vao);
}

void NodePainter::addNode(Node* node) {
    Batch batch = createBatch(node);
    batch.id = nextNodeId_++;
    idToNode_[batch.id] = node;
    batches_[node] = batch;
}

void NodePainter::removeNode(Node* node) {
    auto it = batches_.find(node);
    if (it == batches_.end()) return;
    if (highlightNode_ == node) setHighlight(nullptr, -1);
    idToNode_.erase(it->second.id);
    destroyBatch(it->second);
    batches_.erase(it);
}

Node* NodePainter::nodeForId(uint32_t nodeId) const {
    auto it = idToNode_.find(nodeId);
    return it == idToNode_.end() ? nullptr : it->second;
}

void NodePainter::setHighlight(Node* node, int pointIndex) {
    highlightNode_ = node;
    highlightIndex_ = node ? pointIndex : -1;
}

void NodePainter::paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    glEnable(GL_PROGRAM_POINT_SIZE); // lets the vertex shader drive gl_PointSize for the highlighted point

    shader_.use();
    shader_.setMat4("uMVP", projMatrix * viewMatrix);

    for (auto& [node, batch] : batches_) {
        if (batch.count == 0) continue;
        shader_.setInt("uHighlightIndex", node == highlightNode_ ? highlightIndex_ : -1);
        glBindVertexArray(batch.vao);
        glDrawArrays(GL_POINTS, 0, batch.count);
    }
    glBindVertexArray(0);

    glDisable(GL_PROGRAM_POINT_SIZE);
}

void NodePainter::paintPick(const Mat4f& viewMatrix, const Mat4f& projMatrix) {
    pickShader_.use();
    pickShader_.setMat4("uMVP", projMatrix * viewMatrix);

    for (auto& [node, batch] : batches_) {
        if (batch.count == 0) continue;
        pickShader_.setUint("uNodeId", batch.id);
        glBindVertexArray(batch.vao);
        glDrawArrays(GL_POINTS, 0, batch.count);
    }
    glBindVertexArray(0);
}
