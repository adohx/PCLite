#include <gtest/gtest.h>
#include "camera/orbit_camera.h"
#include "bounding_box.h"
#include <cmath>

static constexpr float kEps = 1e-5f;

// ── Default state ─────────────────────────────────────────────────────────────

TEST(OrbitCameraTest, DefaultPosition) {
    OrbitCamera cam;
    EXPECT_DOUBLE_EQ(cam.position().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.position().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.position().z, 10.0);
}

TEST(OrbitCameraTest, DefaultTarget) {
    OrbitCamera cam;
    EXPECT_DOUBLE_EQ(cam.target().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 0.0);
}

TEST(OrbitCameraTest, DefaultProjectionModePerspective) {
    OrbitCamera cam;
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

TEST(OrbitCameraTest, ViewMatrixStandardLookAt) {
    OrbitCamera cam;
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

// After rotating 90° around Y: looking from (10,0,0) at origin
// right = (0,0,-1), up = (0,1,0), fwd = (-1,0,0)
// Row-major:
//   [ 0, 0,-1, 0]
//   [ 0, 1, 0, 0]
//   [-1, 0, 0,-10]   ← fwd=(1,0,0), -fwd=(-1,0,0); dot(fwd,eye)=10
//   [ 0, 0, 0, 1]
TEST(OrbitCameraTest, ViewMatrixAfterRotate90AroundY) {
    OrbitCamera cam;
    cam.rotate({0, 1, 0}, 90.f);
    auto m = cam.viewMatrix();

    // row 3 must always be [0,0,0,1]
    EXPECT_NEAR(m(3,0), 0.f, kEps); EXPECT_NEAR(m(3,1), 0.f, kEps);
    EXPECT_NEAR(m(3,2), 0.f, kEps); EXPECT_NEAR(m(3,3), 1.f, kEps);

    // The W column encodes -dot(basis, eye): eye=(10,0,0)
    // right=normalize(cross(fwd=(−1,0,0), up=(0,1,0)))=(0,0,−1): dot=(0,0,-1)·(10,0,0)=0
    EXPECT_NEAR(m(0,3), 0.f, 1e-4f);
    // up=(0,1,0): dot=(0,0,0)
    EXPECT_NEAR(m(1,3), 0.f, 1e-4f);
    // -fwd=(1,0,0): m(2,3)=dot(fwd,eye)=(-1,0,0)·(10,0,0)=-10
    EXPECT_NEAR(m(2,3), -10.f, 1e-4f);
}

// ── projectionMatrix ──────────────────────────────────────────────────────────

TEST(OrbitCameraTest, ProjectionMatrixDefaultValues) {
    OrbitCamera cam;
    auto m = cam.projectionMatrix();

    float f  = 1.f / std::tan(60.f * (float)M_PI / 360.f);
    float nf = 0.1f - 1000.f;

    EXPECT_NEAR(m(0,0), f,                             kEps);
    EXPECT_NEAR(m(1,1), f,                             kEps);
    EXPECT_NEAR(m(2,2), (1000.f + 0.1f) / nf,         kEps);
    EXPECT_NEAR(m(2,3), 2.f * 1000.f * 0.1f / nf,     kEps);
    EXPECT_NEAR(m(3,2), -1.f,                          kEps);

    // All other elements must be zero
    EXPECT_NEAR(m(0,1), 0.f, kEps); EXPECT_NEAR(m(0,2), 0.f, kEps);
    EXPECT_NEAR(m(0,3), 0.f, kEps); EXPECT_NEAR(m(1,0), 0.f, kEps);
    EXPECT_NEAR(m(1,2), 0.f, kEps); EXPECT_NEAR(m(1,3), 0.f, kEps);
    EXPECT_NEAR(m(2,0), 0.f, kEps); EXPECT_NEAR(m(2,1), 0.f, kEps);
    EXPECT_NEAR(m(3,0), 0.f, kEps); EXPECT_NEAR(m(3,1), 0.f, kEps);
    EXPECT_NEAR(m(3,3), 0.f, kEps);
}

TEST(OrbitCameraTest, ProjectionMatrixRespectsFov90) {
    OrbitCamera cam;
    cam.setFov(90.f);
    auto m = cam.projectionMatrix();
    // f = 1/tan(45°) = 1.0
    EXPECT_NEAR(m(0,0), 1.f, kEps);
    EXPECT_NEAR(m(1,1), 1.f, kEps);
}

TEST(OrbitCameraTest, ProjectionMatrixRespectsAspectRatio) {
    OrbitCamera cam;
    cam.setAspectRatio(2.f);
    auto m = cam.projectionMatrix();
    float f = 1.f / std::tan(60.f * (float)M_PI / 360.f);
    // m(0,0) = f/aspect, m(1,1) = f
    EXPECT_NEAR(m(0,0), f / 2.f, kEps);
    EXPECT_NEAR(m(1,1), f,       kEps);
}

// ── rotate ────────────────────────────────────────────────────────────────────

// Rotating position (0,0,10) by 90° around Y-axis → (10,0,0)
TEST(OrbitCameraTest, RotateAroundYAxis90) {
    OrbitCamera cam;
    cam.rotate({0, 1, 0}, 90.f);

    EXPECT_NEAR(cam.position().x, 10.0, 1e-5);
    EXPECT_NEAR(cam.position().y,  0.0, 1e-5);
    EXPECT_NEAR(cam.position().z,  0.0, 1e-5);
    // Target unchanged
    EXPECT_DOUBLE_EQ(cam.target().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 0.0);
}

// Two 90° rotations around Y → position (0,0,-10)
TEST(OrbitCameraTest, RotateAroundYAxis180) {
    OrbitCamera cam;
    cam.rotate({0, 1, 0}, 90.f);
    cam.rotate({0, 1, 0}, 90.f);

    EXPECT_NEAR(cam.position().x,   0.0, 1e-4);
    EXPECT_NEAR(cam.position().y,   0.0, 1e-4);
    EXPECT_NEAR(cam.position().z, -10.0, 1e-4);
}

// Zero-length axis is a no-op (guard against division by zero)
TEST(OrbitCameraTest, RotateZeroAxisIsNoOp) {
    OrbitCamera cam;
    auto posBefore = cam.position();
    cam.rotate({0, 0, 0}, 45.f);
    EXPECT_DOUBLE_EQ(cam.position().x, posBefore.x);
    EXPECT_DOUBLE_EQ(cam.position().y, posBefore.y);
    EXPECT_DOUBLE_EQ(cam.position().z, posBefore.z);
}

// ── translate ─────────────────────────────────────────────────────────────────

// translate(1,2,3): position (0,0,10)→(1,2,13), target (0,0,0)→(1,2,3)
TEST(OrbitCameraTest, TranslateShiftsBothPositionAndTarget) {
    OrbitCamera cam;
    cam.translate({1.0, 2.0, 3.0});

    EXPECT_DOUBLE_EQ(cam.position().x,  1.0);
    EXPECT_DOUBLE_EQ(cam.position().y,  2.0);
    EXPECT_DOUBLE_EQ(cam.position().z, 13.0);

    EXPECT_DOUBLE_EQ(cam.target().x, 1.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 2.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 3.0);
}

// Arm length must be preserved after translation
TEST(OrbitCameraTest, TranslatePreservesArmLength) {
    OrbitCamera cam;
    cam.translate({5.0, -3.0, 7.0});
    auto p = cam.position();
    auto t = cam.target();
    double dx = p.x - t.x, dy = p.y - t.y, dz = p.z - t.z;
    EXPECT_NEAR(std::sqrt(dx*dx + dy*dy + dz*dz), 10.0, 1e-9);
}

// ── zoom ──────────────────────────────────────────────────────────────────────

TEST(OrbitCameraTest, ZoomOutDoublesDistance) {
    OrbitCamera cam;
    cam.zoom(2.f);
    EXPECT_NEAR(cam.position().z, 20.0, 1e-5);
}

TEST(OrbitCameraTest, ZoomInHalvesDistance) {
    OrbitCamera cam;
    cam.zoom(0.5f);
    EXPECT_NEAR(cam.position().z, 5.0, 1e-5);
}

TEST(OrbitCameraTest, ZoomZeroIsNoOp) {
    OrbitCamera cam;
    cam.zoom(0.f);
    EXPECT_DOUBLE_EQ(cam.position().z, 10.0);
}

TEST(OrbitCameraTest, ZoomNegativeIsNoOp) {
    OrbitCamera cam;
    cam.zoom(-2.f);
    EXPECT_DOUBLE_EQ(cam.position().z, 10.0);
}

// Zoom does not move target
TEST(OrbitCameraTest, ZoomDoesNotMoveTarget) {
    OrbitCamera cam;
    cam.zoom(5.f);
    EXPECT_DOUBLE_EQ(cam.target().x, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().y, 0.0);
    EXPECT_DOUBLE_EQ(cam.target().z, 0.0);
}

// ── Camera::lookAt(point) ─────────────────────────────────────────────────────

TEST(OrbitCameraTest, LookAtPointSetsTarget) {
    OrbitCamera cam;
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

TEST(OrbitCameraTest, LookAtBBoxCentersCamera) {
    OrbitCamera cam;
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    EXPECT_NEAR(cam.target().x, 0.0, 1e-5);
    EXPECT_NEAR(cam.target().y, 0.0, 1e-5);
    EXPECT_NEAR(cam.target().z, 0.0, 1e-5);
}

TEST(OrbitCameraTest, LookAtBBoxAdjustsDistance) {
    OrbitCamera cam;
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    EXPECT_NEAR(cam.position().x, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().y, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().z, 3.0, 1e-4);
}

TEST(OrbitCameraTest, LookAtBBoxPreservesViewDirection) {
    OrbitCamera cam;
    // Rotate 90° first so eye is at (10,0,0)
    cam.rotate({0, 1, 0}, 90.f);
    BoundingBoxd bbox({-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
    cam.lookAt(bbox);

    // After lookAt the camera should still be looking from the +X direction
    EXPECT_GT(cam.position().x, 0.0);
    EXPECT_NEAR(cam.position().y, 0.0, 1e-4);
    EXPECT_NEAR(cam.position().z, 0.0, 1e-4);
}
