#ifndef PCLITE_WINDOW_H
#define PCLITE_WINDOW_H

#include <memory>
#include <vector>
#include "../viewer/camera/camera.h"
#include "../viewer/layer/layer.h"

class Window {
public:
    virtual ~Window() = default;

    void addLayer(std::unique_ptr<Layer> layer);
    void addCamera(std::unique_ptr<Camera> camera);

    // Drives rendering: sets camera params in context, renders each layer,
    // then composites their outputs into a single frame.
    virtual void render() = 0;

    virtual void onResize(int width, int height);

    int width() const;
    int height() const;

protected:
    std::vector<std::unique_ptr<Layer>>  layers_;
    std::vector<std::unique_ptr<Camera>> cameras_;
    int width_  = 800;
    int height_ = 600;
};

#endif //PCLITE_WINDOW_H
