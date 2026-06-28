#include <gtest/gtest.h>
#include "camera/perspective_camera.h"
#include "bounding_box.h"
#include <cmath>

static constexpr float kEps = 1e-5f;

// ── Default state ─────────────────────────────────────────────────────────────

TEST(PerspectiveCameraTest, DefaultPosition) {
    PerspectiveCamera cam;
    EXPECT_DOUBLE_EQ(cam.position().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.position().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.position().z, 10.0);
}

TEST(PerspectiveCameraTest, DefaultTarget) {
    PerspectiveCamera cam;
    EXPECT_DOUBLE_EQ(cam.target().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 0.0);
}

TEST(PerspectiveCameraTest, DefaultProjectionModePerspective) {
    PerspectiveCamera cam;
    EXPECT_EQ(cam.projectionMode(), ProjectionMode::Perspective);
}

// ── viewMatrix ────────────────────────────────────────────────────────────────
//
// Looking from (0,0,10) at origin with up=(0,1,0):
//   right  = (1,0,0), up_new = (0,1,0), -fwd = (0,0,1)
// Row-major result:
//   [1, 0, 0,   0]
//   [0, 1, 0,   0]
//   [0, 0, 1, -10]
//   [0, 0, 0,   1]

TEST(PerspectiveCameraTest, ViewMatrixStandardLookAt) {
    PerspectiveCamera cam;
    auto m = cam.viewMatrix();

    EXPECT_NEAR(m(0,0),  1.f, kEps);  EXPECT_NEAR(m(0,1), 0.f, kEps);
    EXPECT_NEAR(m(0,2),  0.f, kEps);  EXPECT_NEAR(m(0,3), 0.f, kEps);

    EXPECT_NEAR(m(1,0),  0.f, kEps);  EXPECT_NEAR(m(1,1), 1.f, kEps);
    EXPECT_NEAR(m(1,2),  0.f, kEps);  EXPECT_NEAR(m(1,3), 0.f, kEps);

    EXPECT_NEAR(m(2,0),  0.f, kEps);  EXPECT_NEAR(m(2,1),  0.f, kEps);
    EXPECT_NEAR(m(2,2),  1.f, kEps);  EXPECT_NEAR(m(2,3), -10.f, kEps);

    EXPECT_NEAR(m(3,0),  0.f, kEps);  EXPECT_NEAR(m(3,1), 0.f, kEps);
    EXPECT_NEAR(m(3,2),  0.f, kEps);  EXPECT_NEAR(m(3,3), 1.f, kEps);
}

// Looking from (10,0,0) at origin: right=(0,0,-1), up=(0,1,0), fwd=(-1,0,0)
TEST(PerspectiveCameraTest, ViewMatrixLookFromPosX) {
    PerspectiveCamera cam;
    cam.setPosition({10.0, 0.0, 0.0});
    cam.setTarget({0.0, 0.0, 0.0});
    auto m = cam.viewMatrix();

    EXPECT_NEAR(m(3,0), 0.f, kEps); EXPECT_NEAR(m(3,1), 0.f, kEps);
    EXPECT_NEAR(m(3,2), 0.f, kEps); EXPECT_NEAR(m(3,3), 1.f, kEps);

    // W column encodes -dot(basis, eye); eye=(10,0,0)
    EXPECT_NEAR(m(0,3),   0.f, 1e-4f);  // right=(0,0,-1), dot=(0)
    EXPECT_NEAR(m(1,3),   0.f, 1e-4f);  // up=(0,1,0), dot=(0)
    EXPECT_NEAR(m(2,3), -10.f, 1e-4f);  // -fwd=(1,0,0), dot=10 → -(10)
}

// ── projectionMatrix ──────────────────────────────────────────────────────────

TEST(PerspectiveCameraTest, ProjectionMatrixDefaultValues) {
    PerspectiveCamera cam;
    auto m = cam.projectionMatrix();

    float f  = 1.f / std::tan(60.f * (float)M_PI / 360.f);
    float nf = 0.1f - 1000.f;

    EXPECT_NEAR(m(0,0), f,                             kEps);
    EXPECT_NEAR(m(1,1), f,                             kEps);
    EXPECT_NEAR(m(2,2), (1000.f + 0.1f) / nf,         kEps);
    EXPECT_NEAR(m(2,3), 2.f * 1000.f * 0.1f / nf,     kEps);
    EXPECT_NEAR(m(3,2), -1.f,                          kEps);

    EXPECT_NEAR(m(0,1), 0.f, kEps); EXPECT_NEAR(m(0,2), 0.f, kEps);
    EXPECT_NEAR(m(0,3), 0.f, kEps); EXPECT_NEAR(m(1,0), 0.f, kEps);
    EXPECT_NEAR(m(1,2), 0.f, kEps); EXPECT_NEAR(m(1,3), 0.f, kEps);
    EXPECT_NEAR(m(2,0), 0.f, kEps); EXPECT_NEAR(m(2,1), 0.f, kEps);
    EXPECT_NEAR(m(3,0), 0.f, kEps); EXPECT_NEAR(m(3,1), 0.f, kEps);
    EXPECT_NEAR(m(3,3), 0.f, kEps);
}

TEST(PerspectiveCameraTest, ProjectionMatrixRespectsFov90) {
    PerspectiveCamera cam;
    cam.setFov(90.f);
    auto m = cam.projectionMatrix();
    EXPECT_NEAR(m(0,0), 1.f, kEps);
    EXPECT_NEAR(m(1,1), 1.f, kEps);
}

TEST(PerspectiveCameraTest, ProjectionMatrixRespectsAspectRatio) {
    PerspectiveCamera cam;
    cam.setAspectRatio(2.f);
    auto m = cam.projectionMatrix();
    float f = 1.f / std::tan(60.f * (float)M_PI / 360.f);
    EXPECT_NEAR(m(0,0), f / 2.f, kEps);
    EXPECT_NEAR(m(1,1), f,       kEps);
}

// ── Camera::lookAt(point) ─────────────────────────────────────────────────────

TEST(PerspectiveCameraTest, LookAtPointSetsTarget) {
    PerspectiveCamera cam;
    cam.lookAt(vec3d{5.0, 3.0, 1.0});
    EXPECT_DOUBLE_EQ(cam.target().x, 5.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 3.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 1.0);
}

// ── Camera::lookAt(BoundingBox) ───────────────────────────────────────────────
//
// BBox (-1,-1,-1) to (1,1,1): center=(0,0,0), radius=sqrt(3)
// fov=60°, aspect=1 → halfFovV=halfFovH=30°
// dist = sqrt(3)/tan(30°) = sqrt(3)*sqrt(3) = 3
// arm direction (0,0,10) normalized = (0,0,1)
// → new position = (0, 0, 3)

TEST(PerspectiveCameraTest, LookAtBBoxCentersCamera) {
    PerspectiveCamera cam;
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    EXPECT_NEAR(cam.target().x, 0.0, 1e-5);
    EXPECT_NEAR(cam.target().y, 0.0, 1e-5);
    EXPECT_NEAR(cam.target().z, 0.0, 1e-5);
}

TEST(PerspectiveCameraTest, LookAtBBoxAdjustsDistance) {
    PerspectiveCamera cam;
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    EXPECT_NEAR(cam.position().x, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().y, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().z, 3.0, 1e-4);
}

TEST(PerspectiveCameraTest, LookAtBBoxPreservesViewDirection) {
    PerspectiveCamera cam;
    cam.setPosition({10.0, 0.0, 0.0});
    cam.setTarget({0.0, 0.0, 0.0});
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    EXPECT_GT(cam.position().x, 0.0);
    EXPECT_NEAR(cam.position().y, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().z, 0.0, 1e-4);
}
