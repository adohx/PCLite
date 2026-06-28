#ifndef PCLITE_VIEWPORT_H
#define PCLITE_VIEWPORT_H

#include "window.h"
#include "camera/camera_controller.h"
#include "measurement/measurement_manager.h"
#include "pclite_thread_pool.h"
#include "vec3.h"
#include <cstdint>
#include <future>
#include <memory>
#include <string>

struct Node;
class Layer;
class PointCloudLoader;

// Fixed: orbit always pivots around whatever lookAt()/setTarget() set at
// project open (today's behavior). DoubleClick: double-clicking a point
// re-pivots the controller there (camera position/orientation unchanged at
// that instant -- only where subsequent drags orbit around changes), and it
// stays the pivot until the next double-click. Follow: every left-button
// press re-pivots, without needing a GPU pick/hit at all -- the new pivot
// is just the point along the cursor's view ray at the *same distance* the
// old pivot was, so it works the same whether the press lands on a point or
// on empty space.
enum class RotationCenterMode {
    Fixed,
    DoubleClick,
    Follow,
};

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
        // Locally-fitted-plane info computed for this same pick (the same
        // KD-tree fit behind the cyan ring decal); hasPlane false if the fit
        // failed (e.g. too few neighbors resident). Consumed by
        // MeasurementManager for DistanceMeasurement's reference plane.
        bool hasPlane = false;
        vec3f planeCenter{};
        vec3f planeNormal{};
    };
    const PickResult& lastPick() const { return lastPick_; }

    // Forwards clicks (when hit) to the measurement manager; owned here
    // since Viewport is what resolves clicks to 3D positions via picking.
    MeasurementManager& measurementManager() { return measurementManager_; }
    const MeasurementManager& measurementManager() const { return measurementManager_; }

    // Current measurement's labels, already projected to viewport-local
    // pixel coordinates (origin at this panel's top-left) -- plain
    // data, no ImGui dependency here; MainWindow draws the actual text.
    struct ScreenLabel { float x = 0.f, y = 0.f; std::string text; };
    std::vector<ScreenLabel> measurementScreenLabels() const;

    // Concrete loader used to fetch full-resolution leaf data for pick-assist
    // plane-fit refinement (see queryFinestLeafAt). Not the generic
    // NodeLoader<Node> interface, since this needs PointCloudLoader's
    // specific file-format knowledge; set once by the app at startup with
    // the same loader instance it gave to the LOD strategy.
    void setPickAssistLoader(PointCloudLoader* loader) { pickAssistLoader_ = loader; }

    // Switching back to Fixed drops any pivot set by a previous double-click
    // (controller_->clearRecenter()), so orbiting always reverts cleanly to
    // "around what you're currently looking at" rather than leaving a
    // stale picked pivot in effect under the "Fixed" label.
    void setRotationCenterMode(RotationCenterMode mode) {
        rotationCenterMode_ = mode;
        if (mode == RotationCenterMode::Fixed && controller_) controller_->clearRecenter();
    }
    RotationCenterMode rotationCenterMode() const { return rotationCenterMode_; }

    // Tears down the current project's scene state (layers/cameras/
    // controller/pick state) so the viewport is back to its just-constructed
    // state, ready for a different project to be opened. Waits on any
    // in-flight pick-assist refinement first, since it captures a raw
    // PointCloudLoader* that's about to become invalid.
    void reset();

    void render() override;
    void onResize(int w, int h) override;

    // Input, in panel-local pixel coordinates. MainWindow forwards SDL
    // events here once it has decided they belong to this viewport.
    void onMouseMove(float x, float y);
    // clicks mirrors SDL_MouseButtonEvent::clicks (1 = single, 2 = double,
    // ...); only consulted on press, for double-click-to-recenter.
    void onMouseButton(float x, float y, int button, bool pressed, int clicks = 1);
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

    // Latest hover position waiting to be processed in update() (at most
    // once per frame, however many onMouseMove events arrived); not set
    // while dragging the camera, so hover preview pauses during orbit/pan.
    float hoverX_       = 0.f;
    float hoverY_       = 0.f;
    bool  hoverPending_ = false;

    PickResult lastPick_;
    MeasurementManager measurementManager_;
    RotationCenterMode rotationCenterMode_ = RotationCenterMode::Fixed;

    // Pick-assist plane fit (the ring indicator): computed in two passes.
    // resolvePick() does an immediate fit from whatever's currently
    // resident (cheap, in-memory only, but precision follows the current
    // LOD); a background task then descends to the actual full-resolution
    // leaf node under the cursor and refines the result shortly after,
    // applied whenever it completes. See [[project_point_picking]] for why
    // two passes instead of one.
    //
    // Used for both click-to-pick and hover preview (real-time, updates as
    // the mouse moves): a click launches the background refinement right
    // away; hover launches it only after resting on the same point for
    // kHoverRefineDelay, so sweeping the mouse across many points doesn't
    // flood the refinement thread with disk reads for positions the user
    // has already moved past.
    struct PendingRefinement {
        uint64_t generation = 0; // discarded if a newer pick superseded this one
        bool valid = false;
        Layer* layer = nullptr;
        vec3f center{}, normal{};
        float radius = 0.f;
    };

    uint64_t pickGeneration_ = 0;
    std::future<PendingRefinement> pendingRefinement_;
    PCLiteThreadPool refinementPool_{1};
    PointCloudLoader* pickAssistLoader_ = nullptr;

    // Set by resolvePick() whenever the resolved point actually changes;
    // tracks whether a background refinement has already been launched for
    // whatever's currently displayed, and how long the same point has been
    // continuously hovered (for the settle-before-refining debounce).
    Layer* currentHitLayer_                  = nullptr;
    float  currentSearchRadius_              = 0.f;
    bool   refinementLaunchedForCurrentPick_ = false;
    float  hoverSettleTimer_                 = 0.f;

    void resizeFBO(int w, int h);
    void destroyFBO();

    // Raw result of a single pick-buffer query, with no side effects (no
    // lastPick_/highlight/plane-fit updates) -- used both by resolvePick()
    // and by the rotation-center-follows-cursor press handler, which needs
    // a position under the cursor without disturbing the existing
    // highlight/measurement pick state.
    struct RawPick {
        bool hit = false;
        Node* node = nullptr;
        int pointIndex = -1;
        vec3f position{};
        Layer* layer = nullptr;
    };
    RawPick queryPickBuffer(float x, float y);

    // Shared core for both pick() and previewAt(): renders a small window
    // around (x, y) into pickFbo_, reads back the nearest non-empty pixel,
    // and -- only if the resolved point differs from lastPick_ -- updates
    // lastPick_, the highlight, and the immediate plane fit on every layer.
    // Returns the hit layer (nullptr if no hit), via outRadius the search
    // radius used for the immediate fit.
    Layer* resolvePick(float x, float y, float& outRadius);

    // Click path: resolvePick() + launch background refinement immediately.
    void pick(float x, float y);

    // Hover path: resolvePick() only; background refinement (if any) is
    // launched later from update() once the point has settled.
    void previewAt(float x, float y);

    // Launches the background leaf-descent refinement task for `position`
    // on `layer`, tagged with `generation` so a later, now-stale result
    // gets discarded by applyPendingRefinement().
    void launchRefinement(Layer* layer, const vec3f& position, float radius, uint64_t generation);

    // Applies pendingRefinement_ if it has finished and is still relevant
    // (i.e. no newer pick superseded it since it was launched). Called once
    // per frame from update().
    void applyPendingRefinement();
};

#endif // PCLITE_VIEWPORT_H
