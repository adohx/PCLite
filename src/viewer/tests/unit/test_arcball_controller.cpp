#include <gtest/gtest.h>
#include <cmath>
#include "camera/arcball_controller.h"
#include "camera/perspective_camera.h"

namespace {
double distance(vec3d a, vec3d b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}

TEST(ArcballControllerTest, RecenterToDoesNotChangeCurrentView) {
    PerspectiveCamera cam;
    cam.setPosition({10, 0, 0});
    cam.setTarget({0, 0, 0});

    ArcballController controller;
    controller.syncFromCamera(cam);
    controller.recenterTo({5, 5, 5}); // an arbitrary off-axis point
    controller.applyToCamera(cam);

    // Re-pivoting must never move the camera or change what it's looking
    // at -- only where the *next* drag orbits around.
    EXPECT_NEAR(cam.position().x, 10.0, 1e-9);
    EXPECT_NEAR(cam.position().y, 0.0, 1e-9);
    EXPECT_NEAR(cam.position().z, 0.0, 1e-9);
    EXPECT_NEAR(cam.target().x, 0.0, 1e-9);
    EXPECT_NEAR(cam.target().y, 0.0, 1e-9);
    EXPECT_NEAR(cam.target().z, 0.0, 1e-9);
}

TEST(ArcballControllerTest, RecenterToUpdatesCameraPivotForTheCrosshair) {
    PerspectiveCamera cam;
    cam.setPosition({10, 0, 0});
    cam.setTarget({0, 0, 0});

    ArcballController controller;
    controller.syncFromCamera(cam);
    controller.recenterTo({5, 5, 5});
    controller.applyToCamera(cam);

    // RotationCenterPainter reads Camera::pivot() to draw its crosshair, so
    // this is what actually gives the user visible feedback that
    // double-clicking changed the rotation center.
    EXPECT_NEAR(cam.pivot().x, 5.0, 1e-9);
    EXPECT_NEAR(cam.pivot().y, 5.0, 1e-9);
    EXPECT_NEAR(cam.pivot().z, 5.0, 1e-9);
}

TEST(ArcballControllerTest, DragAfterRecenterOrbitsAroundNewPivot) {
    PerspectiveCamera cam;
    cam.setPosition({10, 0, 0});
    cam.setTarget({0, 0, 0});

    ArcballController controller;
    controller.syncFromCamera(cam);
    controller.onResize(800, 600);
    controller.recenterTo({5, 0, 0});

    controller.onMouseButtonDown(1 /*left*/, 400, 300);
    controller.onMouseDrag(1, 50, 0);
    controller.applyToCamera(cam);

    // Orbiting now preserves distance to the *pivot* (5,0,0), not to the
    // pre-recenter target (0,0,0).
    EXPECT_NEAR(distance(cam.position(), {5, 0, 0}), 5.0, 1e-6);
}

TEST(ArcballControllerTest, DragAfterRecenterAlsoRevolvesTheLookAtTarget) {
    // Rigid-body rotation: target_ revolves around the pivot together with
    // the camera (instead of staying fixed at the pre-recenter target),
    // which is what makes the camera actually orbit "around" the picked
    // point instead of just spinning in place.
    PerspectiveCamera cam;
    cam.setPosition({10, 0, 0});
    cam.setTarget({0, 0, 0});

    ArcballController controller;
    controller.syncFromCamera(cam);
    controller.onResize(800, 600);
    controller.recenterTo({5, 0, 0});

    controller.onMouseButtonDown(1, 400, 300);
    controller.onMouseDrag(1, 50, 0);
    controller.applyToCamera(cam);

    EXPECT_GT(std::abs(cam.target().z), 1e-6); // moved off the original (0,0,0)
}

TEST(ArcballControllerTest, ClearRecenterRevertsToOrbitingTheTarget) {
    PerspectiveCamera cam;
    cam.setPosition({10, 0, 0});
    cam.setTarget({0, 0, 0});

    ArcballController controller;
    controller.syncFromCamera(cam);
    controller.onResize(800, 600);
    controller.recenterTo({5, 0, 0});
    controller.clearRecenter();

    controller.onMouseButtonDown(1, 400, 300);
    controller.onMouseDrag(1, 50, 0);
    controller.applyToCamera(cam);

    // Back to classic behavior: target stays put, camera orbits around it.
    EXPECT_NEAR(cam.target().x, 0.0, 1e-9);
    EXPECT_NEAR(cam.target().y, 0.0, 1e-9);
    EXPECT_NEAR(cam.target().z, 0.0, 1e-9);
    EXPECT_NEAR(distance(cam.position(), cam.target()), 10.0, 1e-6);
}
