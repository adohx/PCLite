#include "viewport.h"
#include "camera/camera.h"
#include "mat.h"
#include "layer/layer.h"
#include "node_management/node_manager.h"
#include "node_loader/point_cloud_loader.h"
#include "../../core/node.h"
#include "../../core/plane_fit.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glad/gl.h>
#include <SDL2/SDL.h>
#include <stdexcept>
#include <vector>

static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

// A left click that moves less than this many pixels between press and
// release is treated as a pick rather than the start/end of an orbit drag.
static constexpr float kClickMoveTolerance = 3.f;

// Pick window half-size, in pixels: tolerates clicking slightly off a thin
// point's exact pixel without resorting to a full-screen pick buffer.
static constexpr int kPickRadius = 4;

// Minimum neighborhood size to attempt a plane fit; below this the PCA
// normal is too noisy to be worth showing.
static constexpr size_t kMinPlaneFitPoints = 6;

namespace {
// Gathers radius-search hits across `nodes`' own KD-trees (each one only
// knows about its own points -- this is the cross-node merge) and fits a
// plane through the combined neighborhood.
bool fitPlaneFromNodes(const std::vector<Node*>& nodes, const vec3f& query, float radius,
                       vec3f& outCenter, vec3f& outNormal) {
    std::vector<vec3f> neighbors;
    for (Node* n : nodes) {
        auto idxs = n->kdTree().radiusSearch(query, radius);
        const auto& pts = n->points();
        for (uint32_t idx : idxs)
            if (idx < pts.size()) neighbors.push_back(pts[idx]);
    }
    if (neighbors.size() < kMinPlaneFitPoints) return false;
    return fitPlane(neighbors, outCenter, outNormal);
}
} // namespace

Viewport::Viewport() = default;

Viewport::~Viewport() {
    destroyFBO();
}

void Viewport::setController(std::unique_ptr<CameraController> controller) {
    controller_ = std::move(controller);
    if (controller_) controller_->onResize(width_, height_);
}

void Viewport::update(float dt) {
    applyPendingRefinement();

    if (!controller_) return;
    controller_->update(dt);
    if (!cameras_.empty()) controller_->applyToCamera(*cameras_[0]);
}

void Viewport::applyPendingRefinement() {
    if (!pendingRefinement_.valid()) return;
    if (pendingRefinement_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    PendingRefinement result = pendingRefinement_.get();
    if (result.generation != pickGeneration_) return; // a newer pick() superseded this one

    if (result.valid && result.layer)
        result.layer->nodeManager().setPlaneFit(true, result.center, result.normal, result.radius);
}

void Viewport::onResize(int w, int h) {
    Window::onResize(w, h);
    if (controller_) controller_->onResize(w, h);
    resizeFBO(w, h);
}

void Viewport::onMouseButton(float x, float y, int button, bool pressed) {
    if (button == SDL_BUTTON_LEFT) {
        if (pressed) {
            pressX_ = x;
            pressY_ = y;
        } else if (leftDragging_) {
            float dx = x - pressX_, dy = y - pressY_;
            if (std::sqrt(dx * dx + dy * dy) <= kClickMoveTolerance) pick(x, y);
        }
        leftDragging_ = pressed;
    }
    if (button == SDL_BUTTON_RIGHT) rightDragging_ = pressed;
    if (pressed) {
        lastMouseX_ = x;
        lastMouseY_ = y;
        if (controller_) controller_->onMouseButtonDown(button, x, y);
    }
}

void Viewport::onMouseMove(float x, float y) {
    if (controller_ && (leftDragging_ || rightDragging_)) {
        float dx = x - lastMouseX_;
        float dy = y - lastMouseY_;
        if (leftDragging_)  controller_->onMouseDrag(kLeftButton,  dx, dy);
        if (rightDragging_) controller_->onMouseDrag(kRightButton, dx, dy);
    }
    lastMouseX_ = x;
    lastMouseY_ = y;
}

void Viewport::onScroll(float delta) {
    if (controller_) controller_->onScroll(delta);
}

void Viewport::onKey(int key, bool pressed) {
    if (controller_) controller_->onKey(key, pressed);
}

void Viewport::resizeFBO(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == fboWidth_ && h == fboHeight_ && fbo_ != 0) return;
    destroyFBO();

    fboWidth_  = w;
    fboHeight_ = h;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    glGenRenderbuffers(1, &depthRenderbuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Viewport framebuffer is incomplete");
    }

    // Pick pass target: integer texture (must use NEAREST filtering -- GL
    // disallows filtering integer formats) holding (nodeId, pointIndex).
    // Built lazily here and only ever rendered into from pick(), not every
    // frame, so its cost is independent of how often the scene redraws.
    glGenFramebuffers(1, &pickFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);

    glGenTextures(1, &pickIdTexture_);
    glBindTexture(GL_TEXTURE_2D, pickIdTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, w, h, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickIdTexture_, 0);

    glGenRenderbuffers(1, &pickDepthRenderbuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, pickDepthRenderbuffer_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pickDepthRenderbuffer_);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Viewport pick framebuffer is incomplete");
}

void Viewport::destroyFBO() {
    if (depthRenderbuffer_)     { glDeleteRenderbuffers(1, &depthRenderbuffer_); depthRenderbuffer_ = 0; }
    if (colorTexture_)          { glDeleteTextures(1, &colorTexture_); colorTexture_ = 0; }
    if (fbo_)                   { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
    if (pickDepthRenderbuffer_) { glDeleteRenderbuffers(1, &pickDepthRenderbuffer_); pickDepthRenderbuffer_ = 0; }
    if (pickIdTexture_)         { glDeleteTextures(1, &pickIdTexture_); pickIdTexture_ = 0; }
    if (pickFbo_)                { glDeleteFramebuffers(1, &pickFbo_); pickFbo_ = 0; }
}

void Viewport::pick(float x, float y) {
    if (pickFbo_ == 0 || cameras_.empty() || layers_.empty()) return;

    int cx = (int)x;
    int cy = fboHeight_ - 1 - (int)y; // GL texture rows are bottom-up; mouse y is top-down
    int x0 = std::max(0, cx - kPickRadius);
    int y0 = std::max(0, cy - kPickRadius);
    int x1 = std::min(fboWidth_  - 1, cx + kPickRadius);
    int y1 = std::min(fboHeight_ - 1, cy + kPickRadius);
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w <= 0 || h <= 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);
    glViewport(0, 0, fboWidth_, fboHeight_);

    GLuint clearId[2] = {0u, 0u}; // nodeId 0 is the "no hit" sentinel
    glClearBufferuiv(GL_COLOR, 0, clearId);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, y0, w, h);

    auto& cam = *cameras_[0];
    Mat4f projMatrix = cam.projectionMatrix();
    Mat4f viewMatrix = cam.viewMatrix();
    for (auto& layer : layers_)
        layer->nodeManager().renderPick(viewMatrix, projMatrix);

    glDisable(GL_SCISSOR_TEST);

    std::vector<uint32_t> pixels((size_t)w * h * 2);
    glReadPixels(x0, y0, w, h, GL_RG_INTEGER, GL_UNSIGNED_INT, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    uint32_t bestNodeId = 0, bestPointIndex = 0;
    int bestDistSq = -1;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            uint32_t nodeId = pixels[((size_t)j * w + i) * 2 + 0];
            if (nodeId == 0) continue;
            int dx = (x0 + i) - cx, dy = (y0 + j) - cy;
            int distSq = dx * dx + dy * dy;
            if (bestDistSq < 0 || distSq < bestDistSq) {
                bestDistSq = distSq;
                bestNodeId = nodeId;
                bestPointIndex = pixels[((size_t)j * w + i) * 2 + 1];
            }
        }
    }

    lastPick_ = PickResult{};
    Layer* hitLayer = nullptr;
    if (bestNodeId != 0) {
        for (auto& layer : layers_) {
            Node* node = layer->nodeManager().nodeForId(bestNodeId);
            if (!node) continue;
            auto points = node->getPoints();
            if ((size_t)bestPointIndex < points.size()) {
                lastPick_.hit        = true;
                lastPick_.node       = node;
                lastPick_.pointIndex = (int)bestPointIndex;
                lastPick_.position   = points[bestPointIndex];
                hitLayer = layer.get();
            }
            break;
        }
    }

    for (auto& layer : layers_)
        layer->nodeManager().setHighlight(lastPick_.node, lastPick_.pointIndex);

    // Pick-assist plane fit, pass 1 (immediate): cross-node merge over
    // whatever's currently resident. Cheap (in-memory only) but precision
    // follows the current LOD -- see [[project_point_picking]].
    ++pickGeneration_;
    uint64_t generation = pickGeneration_;

    bool planeOk = false;
    vec3f planeCenter{}, planeNormal{};
    float searchRadius = 0.f;

    if (lastPick_.hit && hitLayer) {
        searchRadius = std::max(static_cast<float>(lastPick_.node->spacing_ * 4.0), 1e-3f);
        auto candidates = hitLayer->nodeManager().nodesNear(
            vec3d{static_cast<double>(lastPick_.position.x),
                  static_cast<double>(lastPick_.position.y),
                  static_cast<double>(lastPick_.position.z)},
            searchRadius);
        planeOk = fitPlaneFromNodes(candidates, lastPick_.position, searchRadius, planeCenter, planeNormal);
    }

    for (auto& layer : layers_) {
        bool isHitLayer = (layer.get() == hitLayer);
        layer->nodeManager().setPlaneFit(isHitLayer && planeOk, planeCenter, planeNormal, searchRadius);
    }

    // Pick-assist plane fit, pass 2 (background refinement): descend to the
    // actual full-resolution leaf under the cursor regardless of current
    // LOD, off the main thread, and apply it next frame if still relevant.
    // queryFinestLeafAt never touches any live Node, so this can't race the
    // main thread's ordinary LRU-driven loading.
    if (lastPick_.hit && hitLayer && pickAssistLoader_) {
        PointCloudLoader* loader = pickAssistLoader_;
        Layer* layerPtr = hitLayer;
        vec3f queryPos = lastPick_.position;
        float radius = searchRadius;

        pendingRefinement_ = refinementPool_.enqueue([loader, layerPtr, queryPos, radius, generation]() {
            PendingRefinement result;
            result.generation = generation;

            vec3d worldPoint{static_cast<double>(queryPos.x),
                              static_cast<double>(queryPos.y),
                              static_cast<double>(queryPos.z)};
            auto leaf = loader->queryFinestLeafAt(worldPoint);
            if (!leaf.found) return result;

            auto idxs = leaf.kdTree.radiusSearch(queryPos, radius);
            std::vector<vec3f> neighbors;
            neighbors.reserve(idxs.size());
            for (uint32_t idx : idxs)
                if (idx < leaf.points.size()) neighbors.push_back(leaf.points[idx]);

            if (neighbors.size() >= kMinPlaneFitPoints &&
                fitPlane(neighbors, result.center, result.normal)) {
                result.valid  = true;
                result.layer  = layerPtr;
                result.radius = radius;
            }
            return result;
        });
    }
}

void Viewport::render() {
    if (fbo_ == 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, fboWidth_, fboHeight_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!cameras_.empty() && !layers_.empty()) {
        auto& cam = *cameras_[0];
        Mat4f projMatrix = cam.projectionMatrix();
        Mat4f viewMatrix = cam.viewMatrix();

        for (auto& layer : layers_) {
            layer->nodeManager().update(cam);
            layer->nodeManager().render(viewMatrix, projMatrix);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
