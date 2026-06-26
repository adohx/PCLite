#include "window.h"

void Window::addLayer(std::unique_ptr<Layer> layer) {
    layers_.push_back(std::move(layer));
}

void Window::addCamera(std::unique_ptr<Camera> camera) {
    cameras_.push_back(std::move(camera));
}

void Window::onResize(int width, int height) {
    width_  = width;
    height_ = height;
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    for (auto& camera : cameras_)
        camera->setAspectRatio(aspect);
}

int Window::width() const { return width_; }
int Window::height() const { return height_; }
