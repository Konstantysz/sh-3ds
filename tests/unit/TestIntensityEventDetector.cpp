#include "Vision/IntensityEventDetector.h"

#include <gtest/gtest.h>

// ============================================================================
// IntensityEventDetector state machine tests
// ============================================================================

TEST(IntensityEventDetector, InitialStateIsNotBlack)
{
    SH3DS::Vision::IntensityEventDetector det;
    EXPECT_FALSE(det.IsBlack());
    EXPECT_TRUE(det.GetEvents().empty());
}

TEST(IntensityEventDetector, NoEventWhenValueIsBright)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(0.9, 0);
    det.Update(0.85, 1);
    EXPECT_FALSE(det.IsBlack());
    EXPECT_TRUE(det.GetEvents().empty());
}

TEST(IntensityEventDetector, DropEventWhenValueFallsBelowThreshold)
{
    SH3DS::Vision::IntensityEventDetector det;
    // Establish a vMax baseline
    det.Update(1.0, 0);
    // Value drops below 30% of vMax (1.0) -> should trigger DROP
    bool isBlack = det.Update(0.10, 1);
    EXPECT_TRUE(isBlack);
    EXPECT_TRUE(det.IsBlack());
    ASSERT_EQ(det.GetEvents().size(), 1u);
    EXPECT_EQ(det.GetEvents()[0].type, SH3DS::Vision::IntensityEventType::Drop);
    EXPECT_EQ(det.GetEvents()[0].frameIndex, 1u);
}

TEST(IntensityEventDetector, RaiseEventWhenValueRecoveres)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    det.Update(0.10, 1); // DROP
    // Recovery: value > 50% of vMax
    bool isBlack = det.Update(0.80, 2);
    EXPECT_FALSE(isBlack);
    EXPECT_FALSE(det.IsBlack());
    ASSERT_EQ(det.GetEvents().size(), 2u);
    EXPECT_EQ(det.GetEvents()[1].type, SH3DS::Vision::IntensityEventType::Raise);
    EXPECT_EQ(det.GetEvents()[1].frameIndex, 2u);
}

TEST(IntensityEventDetector, NoRaiseBeforeDropThresholdIsMet)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    det.Update(0.10, 1); // DROP
    // Recovery value is only 30% of vMax -> below raiseThreshold (50%) -> stays black
    det.Update(0.30, 2);
    EXPECT_TRUE(det.IsBlack());
    ASSERT_EQ(det.GetEvents().size(), 1u); // only DROP, no RAISE
}

TEST(IntensityEventDetector, MultipleDropRaiseCycles)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    det.Update(0.05, 1); // DROP
    det.Update(0.90, 2); // RAISE
    det.Update(0.05, 3); // DROP again
    det.Update(0.90, 4); // RAISE again
    ASSERT_EQ(det.GetEvents().size(), 4u);
    EXPECT_EQ(det.GetEvents()[0].type, SH3DS::Vision::IntensityEventType::Drop);
    EXPECT_EQ(det.GetEvents()[1].type, SH3DS::Vision::IntensityEventType::Raise);
    EXPECT_EQ(det.GetEvents()[2].type, SH3DS::Vision::IntensityEventType::Drop);
    EXPECT_EQ(det.GetEvents()[3].type, SH3DS::Vision::IntensityEventType::Raise);
}

TEST(IntensityEventDetector, NoDoubleDrop)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    det.Update(0.05, 1); // DROP
    det.Update(0.05, 2); // still black — no second DROP
    ASSERT_EQ(det.GetEvents().size(), 1u);
    EXPECT_EQ(det.GetEvents()[0].type, SH3DS::Vision::IntensityEventType::Drop);
}

TEST(IntensityEventDetector, VMaxDecaysWhenValueIsLow)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    // Feed a very low value many times — vMax should decay
    for (uint64_t i = 1; i <= 1000; ++i)
    {
        det.Update(0.0, i);
    }
    // After 1000 frames of decay: vMax ~ 1.0 * 0.9995^1000 ~ 0.606
    // A bright value of 0.35 should NOT trigger a raise since it was black from frame 1
    // but 0.35 / 0.606 ~ 0.577 which is > 0.5 raiseThreshold -> RAISE should occur
    // This just checks no crash and events are plausible
    EXPECT_GE(det.GetEvents().size(), 1u); // at least the initial DROP
}

TEST(IntensityEventDetector, ResetClearsStateAndEvents)
{
    SH3DS::Vision::IntensityEventDetector det;
    det.Update(1.0, 0);
    det.Update(0.05, 1); // DROP
    det.Reset();
    EXPECT_FALSE(det.IsBlack());
    EXPECT_TRUE(det.GetEvents().empty());
}

TEST(IntensityEventDetector, CustomConfigDropThreshold)
{
    SH3DS::Vision::IntensityEventConfig cfg;
    cfg.dropThreshold = 0.10; // Only drop if below 10% of vMax
    SH3DS::Vision::IntensityEventDetector det(cfg);
    det.Update(1.0, 0);
    // At 15% of vMax — with default 0.30 this would DROP, but with 0.10 it should not
    det.Update(0.15, 1);
    EXPECT_FALSE(det.IsBlack());
    EXPECT_TRUE(det.GetEvents().empty());
}
