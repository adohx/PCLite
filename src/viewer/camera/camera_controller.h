#ifndef PCLITE_CAMERA_CONTROLLER_H
#define PCLITE_CAMERA_CONTROLLER_H

#include "camera.h"

class CameraController {
public:
    virtual ~CameraController() = default;

    // Called by the window when a button is held and the mouse moves.
    virtual void onMouseDrag(int button, float dx, float dy) = 0;

    // Called by the window on scroll wheel movement (delta > 0 = zoom in).
    virtual void onScroll(float delta) = 0;

    // Called by the window when the viewport is resized.
    virtual void onResize(int w, int h) = 0;

    // Initialise controller state from a camera that has already been positioned.
    virtual void syncFromCamera(const Camera& cam) = 0;

    // Push the controller's current state into the camera (called every frame).
    virtual void applyToCamera(Camera& cam) const = 0;
};

#endif // PCLITE_CAMERA_CONTROLLER_H
