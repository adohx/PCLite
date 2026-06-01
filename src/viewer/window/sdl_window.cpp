#include "sdl_window.h"
#include "camera/camera.h"
#include "mat.h"
#include "layer/layer.h"
#include "node_management/node_manager.h"
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

SDLWindow::SDLWindow(int w, int h, const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    title_ = title;
    sdlWindow_ = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!sdlWindow_)
        throw std::runtime_error(SDL_GetError());

    glContext_ = SDL_GL_CreateContext(sdlWindow_);
    SDL_GL_SetSwapInterval(1);

    width_  = w;
    height_ = h;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.08f, 0.12f, 1.f);
    glPointSize(2.f);
}

SDLWindow::~SDLWindow() {
    if (glContext_)  SDL_GL_DeleteContext(glContext_);
    if (sdlWindow_)  SDL_DestroyWindow(sdlWindow_);
    SDL_Quit();
}

void SDLWindow::setController(std::unique_ptr<CameraController> controller) {
    controller_ = std::move(controller);
    if (controller_) controller_->onResize(width_, height_);
}

void SDLWindow::onResize(int w, int h) {
    Window::onResize(w, h);
    if (controller_) controller_->onResize(w, h);
}

void SDLWindow::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_QUIT:
        running_ = false;
        break;

    case SDL_KEYDOWN:
        if (e.key.keysym.sym == SDLK_ESCAPE) running_ = false;
        onKeyEvent(e.key.keysym.sym, true);
        break;

    case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftDragging_ = true;
            lastMouseX_   = e.button.x;
            lastMouseY_   = e.button.y;
        }
        if (e.button.button == SDL_BUTTON_RIGHT) {
            rightDragging_ = true;
            lastMouseX_    = e.button.x;
            lastMouseY_    = e.button.y;
        }
        onMouseButton((float)e.button.x, (float)e.button.y, e.button.button, true);
        break;

    case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT)  leftDragging_  = false;
        if (e.button.button == SDL_BUTTON_RIGHT) rightDragging_ = false;
        onMouseButton((float)e.button.x, (float)e.button.y, e.button.button, false);
        break;

    case SDL_MOUSEMOTION:
        if ((leftDragging_ || rightDragging_) && controller_) {
            float dx = (float)(e.motion.x - lastMouseX_);
            float dy = (float)(e.motion.y - lastMouseY_);
            if (leftDragging_)  controller_->onMouseDrag(SDL_BUTTON_LEFT,  dx, dy);
            if (rightDragging_) controller_->onMouseDrag(SDL_BUTTON_RIGHT, dx, dy);
        }
        lastMouseX_ = e.motion.x;
        lastMouseY_ = e.motion.y;
        onMouseMove((float)e.motion.x, (float)e.motion.y);
        break;

    case SDL_MOUSEWHEEL:
        if (controller_) controller_->onScroll((float)e.wheel.y);
        onMouseScroll((float)e.wheel.y);
        break;

    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_RESIZED)
            onResize(e.window.data1, e.window.data2);
        break;
    }
}

void SDLWindow::render() {
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (cameras_.empty() || layers_.empty()) return;
    auto& cam = *cameras_[0];

    glMatrixMode(GL_PROJECTION);
    auto proj = cam.projectionMatrix().data();
    glLoadMatrixf(proj.data());
    glMatrixMode(GL_MODELVIEW);
    Mat4f viewMatrix = cam.viewMatrix();
    auto viewArr = viewMatrix.data();
    glLoadMatrixf(viewArr.data());

    for (auto& layer : layers_) {
        layer->nodeManager().update(cam);
        layer->nodeManager().render(viewMatrix);
    }
}

void SDLWindow::run() {
    using Clock = std::chrono::steady_clock;
    auto fpsTimer = Clock::now();
    int frameCount = 0;

    running_ = true;
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handleEvent(e);

        if (controller_ && !cameras_.empty())
            controller_->applyToCamera(*cameras_[0]);

        render();
        SDL_GL_SwapWindow(sdlWindow_);

        ++frameCount;
        auto now = Clock::now();
        float elapsed = std::chrono::duration<float>(now - fpsTimer).count();
        if (elapsed >= 1.0f) {
            float fps = frameCount / elapsed;
            SDL_SetWindowTitle(sdlWindow_,
                (title_ + "  |  FPS: " + std::to_string(static_cast<int>(fps))).c_str());
            fpsTimer = now;
            frameCount = 0;
        }
    }
}
