#ifndef PCLITE_CAMERA_CONTROLLER_H
#define PCLITE_CAMERA_CONTROLLER_H

#include "camera.h"

class CameraController {
public:
    virtual ~CameraController() = default;

    // Called by the window when a mouse button is first pressed.
    virtual void onMouseButtonDown(int /*button*/, float /*x*/, float /*y*/) {}

    // Called by the window when a button is held and the mouse moves.
    virtual void onMouseDrag(int button, float dx, float dy) = 0;

    // Re-pivots the controller's orbit center to `point` without moving the
    // camera's current position/orientation (only where it orbits around
    // next changes). No-op for controllers that don't have an orbit pivot
    // (e.g. a first-person controller).
    virtual void recenterTo(vec3d /*point*/) {}

    // Drops any recenterTo() override, going back to orbiting around
    // wherever the camera is currently looking. No-op for controllers that
    // don't have an orbit pivot.
    virtual void clearRecenter() {}

    // Called by the window on scroll wheel movement (delta > 0 = zoom in).
    virtual void onScroll(float delta) = 0;

    // Called by the window when the viewport is resized.
    virtual void onResize(int w, int h) = 0;

    // Initialise controller state from a camera that has already been positioned.
    virtual void syncFromCamera(const Camera& cam) = 0;

    // Called every frame before applyToCamera; dt is seconds since last frame.
    virtual void update(float /*dt*/) {}

    // Called by the window on key press / release.
    virtual void onKey(int /*key*/, bool /*pressed*/) {}

    // Push the controller's current state into the camera (called every frame).
    virtual void applyToCamera(Camera& cam) const = 0;
};

#endif // PCLITE_CAMERA_CONTROLLER_H
