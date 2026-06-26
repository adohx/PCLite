#ifndef PCLITE_VIEWPORT_H
#define PCLITE_VIEWPORT_H

#include "window.h"
#include "camera/camera_controller.h"
#include <memory>

// Renders the point-cloud scene into an offscreen texture (rather than
// directly to the screen) so MainWindow can display it inside a dockable
// ImGui panel alongside the toolbar/status bar/property panels.
class Viewport : public Window {
public:
    Viewport();
    ~Viewport() override;

    void render() override;
    void onResize(int w, int h) override;

    // Input, in panel-local pixel coordinates. MainWindow forwards SDL
    // events here once it has decided they belong to this viewport.
    void onMouseMove(float x, float y);
    void onMouseButton(float x, float y, int button, bool pressed);
    void onScroll(float delta);
    void onKey(int key, bool pressed);

    // Takes ownership of the controller and syncs its screen size.
    void setController(std::unique_ptr<CameraController> controller);

    // Advances the controller and applies it to camera[0]; call once per frame.
    void update(float dt);

    // Color attachment of the offscreen framebuffer, suitable for ImGui::Image.
    unsigned int textureId() const { return colorTexture_; }

private:
    std::unique_ptr<CameraController> controller_;

    unsigned int fbo_               = 0;
    unsigned int colorTexture_      = 0;
    unsigned int depthRenderbuffer_ = 0;
    int          fboWidth_          = 0;
    int          fboHeight_         = 0;

    bool  leftDragging_  = false;
    bool  rightDragging_ = false;
    float lastMouseX_    = 0.f;
    float lastMouseY_    = 0.f;

    void resizeFBO(int w, int h);
    void destroyFBO();
};

#endif // PCLITE_VIEWPORT_H
