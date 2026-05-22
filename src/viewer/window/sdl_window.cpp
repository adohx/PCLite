#include "sdl_window.h"
#include "camera/camera.h"
#include "mat.h"
#include "layer/layer.h"
#include "node_management/node_manager.h"
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

SDLWindow::SDLWindow(int w, int h, const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

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

void SDLWindow::syncOrbitFromCamera() {
    if (cameras_.empty()) return;
    auto& cam = *cameras_[0];
    auto pos = cam.position();
    auto tgt = cam.target();

    float dx = (float)(pos.x - tgt.x);
    float dy = (float)(pos.y - tgt.y);
    float dz = (float)(pos.z - tgt.z);

    distance_  = std::sqrt(dx*dx + dy*dy + dz*dz);
    elevation_ = std::asin(std::clamp(dy / distance_, -1.f, 1.f)) * 180.f / (float)M_PI;
    azimuth_   = std::atan2(dx, dz) * 180.f / (float)M_PI;
}

void SDLWindow::updateCameraPosition() {
    if (cameras_.empty()) return;
    auto& cam = *cameras_[0];

    auto tgt = cam.target();
    float az = azimuth_   * (float)M_PI / 180.f;
    float el = elevation_ * (float)M_PI / 180.f;

    cam.setPosition({
        tgt.x + distance_ * std::cos(el) * std::sin(az),
        tgt.y + distance_ * std::sin(el),
        tgt.z + distance_ * std::cos(el) * std::cos(az)
    });
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
            dragging_   = true;
            lastMouseX_ = e.button.x;
            lastMouseY_ = e.button.y;
        }
        onMouseButton((float)e.button.x, (float)e.button.y, e.button.button, true);
        break;

    case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) dragging_ = false;
        onMouseButton((float)e.button.x, (float)e.button.y, e.button.button, false);
        break;

    case SDL_MOUSEMOTION:
        if (dragging_) {
            float dx = (float)(e.motion.x - lastMouseX_);
            float dy = (float)(e.motion.y - lastMouseY_);
            azimuth_   -= dx * 0.4f;
            elevation_  = std::clamp(elevation_ + dy * 0.4f, -89.f, 89.f);
            lastMouseX_ = e.motion.x;
            lastMouseY_ = e.motion.y;
        }
        onMouseMove((float)e.motion.x, (float)e.motion.y);
        break;

    case SDL_MOUSEWHEEL:
        distance_ *= (e.wheel.y > 0) ? 0.85f : 1.18f;
        distance_  = std::clamp(distance_, 0.5f, 5000.f);
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
    auto proj = cam.projectionMatrix().data();  // column-major for OpenGL
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
    running_ = true;
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handleEvent(e);

        updateCameraPosition();
        render();
        SDL_GL_SwapWindow(sdlWindow_);
    }
}
