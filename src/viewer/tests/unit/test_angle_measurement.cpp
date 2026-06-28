#include <gtest/gtest.h>
#include "measurement/angle_measurement.h"
#include "measurement/angle_measurement_display.h"

TEST(AngleMeasurementTest, IncompleteBeforeThirdPoint) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    EXPECT_EQ(m.pointCount(), 2);
    EXPECT_FALSE(m.isComplete());
}

TEST(AngleMeasurementTest, SecondClickIsTheVertex) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({5, 5, 5}, false, {}, {}); // vertex
    m.addPoint({0, 1, 0}, false, {}, {});

    EXPECT_NEAR(m.vertex().x, 5.f, 1e-4f);
    EXPECT_NEAR(m.vertex().y, 5.f, 1e-4f);
    EXPECT_NEAR(m.vertex().z, 5.f, 1e-4f);
}

TEST(AngleMeasurementTest, RightAngle) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({0, 1, 0}, false, {}, {});
    ASSERT_TRUE(m.isComplete());
    EXPECT_NEAR(m.angleDegrees(), 90.f, 1e-3f);
}

TEST(AngleMeasurementTest, ZeroAngleForCoincidentRays) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({2, 0, 0}, false, {}, {});
    ASSERT_TRUE(m.isComplete());
    EXPECT_NEAR(m.angleDegrees(), 0.f, 1e-3f);
}

TEST(AngleMeasurementTest, IgnoresPointsPastCompletion) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({0, 1, 0}, false, {}, {});
    m.addPoint({9, 9, 9}, false, {}, {});
    EXPECT_EQ(m.pointCount(), 3);
    EXPECT_NEAR(m.angleDegrees(), 90.f, 1e-3f);
}

TEST(AngleMeasurementDisplayTest, OneRayBeforeComplete) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    AngleMeasurementDisplay display(&m);
    EXPECT_EQ(display.lines().size(), 1u);
    EXPECT_TRUE(display.labels().empty());
}

TEST(AngleMeasurementDisplayTest, RaysPlusArcAndLabelWhenComplete) {
    AngleMeasurement m;
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({0, 1, 0}, false, {}, {});
    AngleMeasurementDisplay display(&m);
    EXPECT_GT(display.lines().size(), 2u); // 2 rays + arc segments
    ASSERT_EQ(display.labels().size(), 1u);
    EXPECT_NE(display.labels()[0].text.find("90"), std::string::npos);
}
