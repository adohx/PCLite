#include <gtest/gtest.h>
#include <cmath>
#include "measurement/distance_measurement.h"
#include "measurement/distance_measurement_display.h"

TEST(DistanceMeasurementTest, IncompleteAfterOnePoint) {
    DistanceMeasurement m;
    m.addPoint({1, 2, 3}, false, {}, {});
    EXPECT_EQ(m.pointCount(), 1);
    EXPECT_FALSE(m.isComplete());
}

TEST(DistanceMeasurementTest, StraightDistanceWithoutPlane) {
    DistanceMeasurement m;
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({3, 4, 0}, false, {}, {}); // plane fit failed at point2
    EXPECT_TRUE(m.isComplete());
    EXPECT_FALSE(m.hasFoot());
    EXPECT_NEAR(m.pointToPointDistance(), 5.f, 1e-4f);
}

TEST(DistanceMeasurementTest, DecomposesAgainstPlaneAtPoint2) {
    DistanceMeasurement m;
    m.addPoint({1, 2, 5}, false, {}, {});                          // point1: measured point
    m.addPoint({0, 0, 0}, true, {0, 0, 0}, {0, 0, 1});              // point2: plane anchor, plane z=0

    ASSERT_TRUE(m.isComplete());
    ASSERT_TRUE(m.hasFoot());

    EXPECT_NEAR(m.foot().x, 1.f, 1e-4f);
    EXPECT_NEAR(m.foot().y, 2.f, 1e-4f);
    EXPECT_NEAR(m.foot().z, 0.f, 1e-4f);

    EXPECT_NEAR(m.perpendicularDistance(), 5.f, 1e-4f);           // point1 to plane
    EXPECT_NEAR(m.inPlaneDistance(), std::sqrt(5.f), 1e-4f);       // foot to point2
    EXPECT_NEAR(m.pointToPointDistance(), std::sqrt(30.f), 1e-4f); // point1 to point2
}

TEST(DistanceMeasurementTest, IgnoresPointsPastCompletion) {
    DistanceMeasurement m;
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({1, 0, 0}, false, {}, {});
    m.addPoint({9, 9, 9}, false, {}, {}); // no-op, already complete
    EXPECT_EQ(m.pointCount(), 2);
    EXPECT_NEAR(m.pointToPointDistance(), 1.f, 1e-4f);
}

TEST(DistanceMeasurementDisplayTest, NoLinesBeforeSecondPoint) {
    DistanceMeasurement m;
    m.addPoint({0, 0, 0}, false, {}, {});
    DistanceMeasurementDisplay display(&m);
    EXPECT_TRUE(display.lines().empty());
    EXPECT_TRUE(display.labels().empty());
}

TEST(DistanceMeasurementDisplayTest, OneLineWithoutPlane) {
    DistanceMeasurement m;
    m.addPoint({0, 0, 0}, false, {}, {});
    m.addPoint({3, 4, 0}, false, {}, {});
    DistanceMeasurementDisplay display(&m);
    EXPECT_EQ(display.lines().size(), 1u);
    EXPECT_EQ(display.labels().size(), 1u);
}

TEST(DistanceMeasurementDisplayTest, ThreeLinesWithPlane) {
    DistanceMeasurement m;
    m.addPoint({1, 2, 5}, false, {}, {});
    m.addPoint({0, 0, 0}, true, {0, 0, 0}, {0, 0, 1});
    DistanceMeasurementDisplay display(&m);
    EXPECT_EQ(display.lines().size(), 3u);
    EXPECT_EQ(display.labels().size(), 3u);
}
