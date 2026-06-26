#include "viewport.h"
#include "camera/camera.h"
#include "mat.h"
#include "layer/layer.h"
#include "node_management/node_manager.h"
#include <glad/gl.h>
#include <SDL2/SDL.h>
#include <stdexcept>

static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

Viewport::Viewport() = default;

Viewport::~Viewport() {
    destroyFBO();
}

void Viewport::setController(std::unique_ptr<CameraController> controller) {
    controller_ = std::move(controller);
    if (controller_) controller_->onResize(width_, height_);
}

void Viewport::update(float dt) {
    if (!controller_) return;
    controller_->update(dt);
    if (!cameras_.empty()) controller_->applyToCamera(*cameras_[0]);
}

void Viewport::onResize(int w, int h) {
    Window::onResize(w, h);
    if (controller_) controller_->onResize(w, h);
    resizeFBO(w, h);
}

void Viewport::onMouseButton(float x, float y, int button, bool pressed) {
    if (button == SDL_BUTTON_LEFT)  leftDragging_  = pressed;
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Viewport framebuffer is incomplete");
}

void Viewport::destroyFBO() {
    if (depthRenderbuffer_) { glDeleteRenderbuffers(1, &depthRenderbuffer_); depthRenderbuffer_ = 0; }
    if (colorTexture_)      { glDeleteTextures(1, &colorTexture_); colorTexture_ = 0; }
    if (fbo_)               { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
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
