#include <gtest/gtest.h>
#include "measurement/measurement_manager.h"

TEST(MeasurementManagerTest, NoOpWhenModeIsNone) {
    MeasurementManager mgr;
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    mgr.onPointPicked({1, 0, 0}, false, {}, {});
    EXPECT_TRUE(mgr.currentLines().empty());
}

TEST(MeasurementManagerTest, DistanceModeAccumulatesTwoPoints) {
    MeasurementManager mgr;
    mgr.setMode(MeasurementMode::Distance);
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    EXPECT_TRUE(mgr.currentLines().empty()); // only 1 point so far

    mgr.onPointPicked({3, 4, 0}, false, {}, {});
    EXPECT_EQ(mgr.currentLines().size(), 1u); // straight line only, no plane
}

TEST(MeasurementManagerTest, NewClickAfterCompletionStartsFreshMeasurement) {
    MeasurementManager mgr;
    mgr.setMode(MeasurementMode::Distance);
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    mgr.onPointPicked({3, 4, 0}, false, {}, {});
    ASSERT_EQ(mgr.currentLines().size(), 1u);

    mgr.onPointPicked({10, 10, 10}, false, {}, {}); // 3rd click: starts a new measurement
    EXPECT_TRUE(mgr.currentLines().empty());        // new one only has 1 point so far
}

TEST(MeasurementManagerTest, SetModeDiscardsCurrentMeasurement) {
    MeasurementManager mgr;
    mgr.setMode(MeasurementMode::Distance);
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    mgr.onPointPicked({3, 4, 0}, false, {}, {});
    ASSERT_EQ(mgr.currentLines().size(), 1u);

    mgr.setMode(MeasurementMode::Angle);
    EXPECT_TRUE(mgr.currentLines().empty());
    EXPECT_EQ(mgr.mode(), MeasurementMode::Angle);
}

TEST(MeasurementManagerTest, ClearKeepsModeButDiscardsMeasurement) {
    MeasurementManager mgr;
    mgr.setMode(MeasurementMode::Distance);
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    mgr.onPointPicked({3, 4, 0}, false, {}, {});
    ASSERT_EQ(mgr.currentLines().size(), 1u);

    mgr.clear();
    EXPECT_TRUE(mgr.currentLines().empty());
    EXPECT_EQ(mgr.mode(), MeasurementMode::Distance);
}

TEST(MeasurementManagerTest, AngleModeCompletesAfterThreePoints) {
    MeasurementManager mgr;
    mgr.setMode(MeasurementMode::Angle);
    mgr.onPointPicked({1, 0, 0}, false, {}, {});
    mgr.onPointPicked({0, 0, 0}, false, {}, {});
    mgr.onPointPicked({0, 1, 0}, false, {}, {});
    EXPECT_GT(mgr.currentLines().size(), 2u);
    ASSERT_EQ(mgr.currentLabels().size(), 1u);
    EXPECT_NE(mgr.currentLabels()[0].text.find("90"), std::string::npos);
}
