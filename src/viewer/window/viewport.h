#ifndef PCLITE_VIEWPORT_H
#define PCLITE_VIEWPORT_H

#include "window.h"
#include "camera/camera_controller.h"
#include "pclite_thread_pool.h"
#include "vec3.h"
#include <cstdint>
#include <future>
#include <memory>

struct Node;
class Layer;
class PointCloudLoader;

// Renders the point-cloud scene into an offscreen texture (rather than
// directly to the screen) so MainWindow can display it inside a dockable
// ImGui panel alongside the toolbar/status bar/property panels.
class Viewport : public Window {
public:
    Viewport();
    ~Viewport() override;

    // Result of the most recent click-to-pick; node == nullptr means the
    // click didn't land on a point (or nothing has been picked yet).
    struct PickResult {
        bool hit = false;
        Node* node = nullptr;
        int pointIndex = -1;
        vec3f position{};
    };
    const PickResult& lastPick() const { return lastPick_; }

    // Concrete loader used to fetch full-resolution leaf data for pick-assist
    // plane-fit refinement (see queryFinestLeafAt). Not the generic
    // NodeLoader<Node> interface, since this needs PointCloudLoader's
    // specific file-format knowledge; set once by the app at startup with
    // the same loader instance it gave to the LOD strategy.
    void setPickAssistLoader(PointCloudLoader* loader) { pickAssistLoader_ = loader; }

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

    // Pick pass render target: same size as the color FBO, two-channel
    // unsigned-int texture holding (nodeId, pointIndex) per pixel. Only
    // rendered into on demand (a left click that isn't a drag), never
    // every frame -- see the picking design discussion for why.
    unsigned int pickFbo_               = 0;
    unsigned int pickIdTexture_         = 0;
    unsigned int pickDepthRenderbuffer_ = 0;

    bool  leftDragging_  = false;
    bool  rightDragging_ = false;
    float lastMouseX_    = 0.f;
    float lastMouseY_    = 0.f;
    float pressX_        = 0.f;
    float pressY_        = 0.f;

    PickResult lastPick_;

    // Pick-assist plane fit (the ring indicator): computed in two passes.
    // pick() does an immediate fit from whatever's currently resident
    // (cheap, in-memory only, but precision follows the current LOD); a
    // background task then descends to the actual full-resolution leaf
    // node under the cursor and refines the result a frame or two later.
    // See [[project_point_picking]] for why two passes instead of one.
    struct PendingRefinement {
        uint64_t generation = 0; // discarded if a newer pick() happened meanwhile
        bool valid = false;
        Layer* layer = nullptr;
        vec3f center{}, normal{};
        float radius = 0.f;
    };

    uint64_t pickGeneration_ = 0;
    std::future<PendingRefinement> pendingRefinement_;
    PCLiteThreadPool refinementPool_{1};
    PointCloudLoader* pickAssistLoader_ = nullptr;

    void resizeFBO(int w, int h);
    void destroyFBO();

    // Renders a small window around (x, y) into pickFbo_ and reads back
    // the nearest non-empty pixel, updating lastPick_ and the highlighted
    // point on every layer.
    void pick(float x, float y);

    // Applies pendingRefinement_ if it has finished and is still relevant
    // (i.e. no newer pick() happened since it was launched). Called once
    // per frame from update().
    void applyPendingRefinement();
};

#endif // PCLITE_VIEWPORT_H
