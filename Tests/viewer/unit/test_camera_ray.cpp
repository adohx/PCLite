#include <gtest/gtest.h>
#include <cmath>
#include "camera/camera_ray.h"

TEST(CameraRayTest, CenterPixelMatchesForwardDirection) {
    vec3d dir = cameraScreenRayDirection({0, 0, 0}, {0, 0, 1}, {0, 1, 0},
                                         60.f, 1.f, 400.f, 300.f, 800.f, 600.f);
    EXPECT_NEAR(dir.x, 0.0, 1e-6);
    EXPECT_NEAR(dir.y, 0.0, 1e-6);
    EXPECT_NEAR(dir.z, 1.0, 1e-6);
}

TEST(CameraRayTest, DirectionIsUnitLength) {
    vec3d dir = cameraScreenRayDirection({0, 0, 0}, {0, 0, 1}, {0, 1, 0},
                                         60.f, 1.33f, 100.f, 50.f, 800.f, 600.f);
    double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    EXPECT_NEAR(len, 1.0, 1e-6);
}

TEST(CameraRayTest, TopEdgePixelDeviatesByHalfVerticalFov) {
    float fov = 90.f; // halfFovY = 45 deg
    vec3d dir = cameraScreenRayDirection({0, 0, 0}, {0, 0, 1}, {0, 1, 0},
                                         fov, 1.f, 400.f /*center column*/, 0.f /*top row*/, 800.f, 600.f);
    double angleFromForward = std::acos(dir.z); // forward == (0,0,1), dir is unit length
    EXPECT_NEAR(angleFromForward, 45.0 * M_PI / 180.0, 1e-3);
}

TEST(CameraRayTest, WiderAspectWidensHorizontalSpreadOnly) {
    vec3d narrow = cameraScreenRayDirection({0, 0, 0}, {0, 0, 1}, {0, 1, 0},
                                            60.f, 1.f, 800.f, 300.f, 800.f, 600.f);
    vec3d wide = cameraScreenRayDirection({0, 0, 0}, {0, 0, 1}, {0, 1, 0},
                                          60.f, 2.f, 800.f, 300.f, 800.f, 600.f);
    // Same rightmost pixel, wider aspect -> ray points further off-axis horizontally.
    EXPECT_GT(std::abs(wide.x), std::abs(narrow.x));
}
