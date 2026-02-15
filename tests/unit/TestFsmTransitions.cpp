#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/ConfigDrivenFSM.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

#include <thread>

namespace
{

    SH3DS::Core::GameProfile CreateTestProfile()
    {
        SH3DS::Core::GameProfile profile;
        profile.gameId = "test_game";
        profile.gameName = "Test Game";
        profile.initialState = "unknown";
        profile.debounceFrames = 2;

        // State: dark_screen — detected by mostly dark pixels
        SH3DS::Core::StateDefinition darkScreen;
        darkScreen.id = "dark_screen";
        darkScreen.description = "Dark/black screen";
        darkScreen.detection.method = "color_histogram";
        darkScreen.detection.roi = "full_screen";
        darkScreen.detection.hsvLower = cv::Scalar(0, 0, 0);
        darkScreen.detection.hsvUpper = cv::Scalar(180, 50, 50);
        darkScreen.detection.pixelRatioMin = 0.8;
        darkScreen.detection.pixelRatioMax = 1.0;
        darkScreen.detection.threshold = 0.5;
        darkScreen.maxDurationS = 10;
        profile.states.push_back(darkScreen);

        // State: bright_screen — detected by mostly bright pixels
        SH3DS::Core::StateDefinition brightScreen;
        brightScreen.id = "bright_screen";
        brightScreen.description = "Bright/white screen";
        brightScreen.detection.method = "color_histogram";
        brightScreen.detection.roi = "full_screen";
        brightScreen.detection.hsvLower = cv::Scalar(0, 0, 200);
        brightScreen.detection.hsvUpper = cv::Scalar(180, 50, 255);
        brightScreen.detection.pixelRatioMin = 0.8;
        brightScreen.detection.pixelRatioMax = 1.0;
        brightScreen.detection.threshold = 0.5;
        brightScreen.maxDurationS = 10;
        brightScreen.shinyCheck = true;
        profile.states.push_back(brightScreen);

        return profile;
    }

    SH3DS::Core::ROISet CreateDarkROI()
    {
        SH3DS::Core::ROISet rois;
        rois["full_screen"] = cv::Mat(240, 400, CV_8UC3, cv::Scalar(10, 10, 10));
        return rois;
    }

    SH3DS::Core::ROISet CreateBrightROI()
    {
        SH3DS::Core::ROISet rois;
        rois["full_screen"] = cv::Mat(240, 400, CV_8UC3, cv::Scalar(240, 240, 240));
        return rois;
    }

    SH3DS::Core::ROISet CreateMidtoneROI()
    {
        SH3DS::Core::ROISet rois;
        rois["full_screen"] = cv::Mat(240, 400, CV_8UC3, cv::Scalar(128, 128, 128));
        return rois;
    }

} // namespace

TEST(ConfigDrivenFSM, InitialStateIsFromProfile)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);
    EXPECT_EQ(fsm.CurrentState(), "unknown");
}

TEST(ConfigDrivenFSM, DetectsDarkScreen)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();

    // Need debounceFrames (2) consecutive detections
    auto t1 = fsm.Update(darkRoi);
    EXPECT_FALSE(t1.has_value()); // First frame — pending

    auto t2 = fsm.Update(darkRoi);
    ASSERT_TRUE(t2.has_value()); // Second frame — transition!
    EXPECT_EQ(t2->from, "unknown");
    EXPECT_EQ(t2->to, "dark_screen");
    EXPECT_EQ(fsm.CurrentState(), "dark_screen");
}

TEST(ConfigDrivenFSM, DetectsBrightScreen)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto brightRoi = CreateBrightROI();

    fsm.Update(brightRoi);
    auto t = fsm.Update(brightRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "bright_screen");
}

TEST(ConfigDrivenFSM, TransitionsFromDarkToBright)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    // Transition to dark_screen
    fsm.Update(darkRoi);
    fsm.Update(darkRoi);
    EXPECT_EQ(fsm.CurrentState(), "dark_screen");

    // Transition to bright_screen
    fsm.Update(brightRoi);
    auto t = fsm.Update(brightRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->from, "dark_screen");
    EXPECT_EQ(t->to, "bright_screen");
}

TEST(ConfigDrivenFSM, DebouncePreventsSingleFrameTransition)
{
    auto profile = CreateTestProfile();
    profile.debounceFrames = 3;
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();

    EXPECT_FALSE(fsm.Update(darkRoi).has_value()); // Frame 1
    EXPECT_FALSE(fsm.Update(darkRoi).has_value()); // Frame 2
    EXPECT_TRUE(fsm.Update(darkRoi).has_value());  // Frame 3 — transition
}

TEST(ConfigDrivenFSM, DebounceResetsOnDifferentState)
{
    auto profile = CreateTestProfile();
    profile.debounceFrames = 3;
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    fsm.Update(darkRoi);                           // dark frame 1
    fsm.Update(darkRoi);                           // dark frame 2
    fsm.Update(brightRoi);                         // bright frame — resets dark debounce
    EXPECT_FALSE(fsm.Update(darkRoi).has_value()); // dark frame 1 again (not 3)
}

TEST(ConfigDrivenFSM, StaysInCurrentStateWhenNoMatch)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    // Midtone ROI shouldn't match either dark or bright
    auto midRoi = CreateMidtoneROI();
    EXPECT_FALSE(fsm.Update(midRoi).has_value());
    EXPECT_EQ(fsm.CurrentState(), "unknown");
}

TEST(ConfigDrivenFSM, ForceStateChangesImmediately)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    fsm.ForceState("bright_screen");
    EXPECT_EQ(fsm.CurrentState(), "bright_screen");
}

TEST(ConfigDrivenFSM, ResetGoesBackToInitialState)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    fsm.ForceState("dark_screen");
    fsm.Reset();
    EXPECT_EQ(fsm.CurrentState(), "unknown");
    EXPECT_TRUE(fsm.History().empty());
}

TEST(ConfigDrivenFSM, HistoryRecordsTransitions)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    fsm.Update(darkRoi);
    fsm.Update(darkRoi); // -> dark_screen
    fsm.Update(brightRoi);
    fsm.Update(brightRoi); // -> bright_screen

    ASSERT_EQ(fsm.History().size(), 2u);
    EXPECT_EQ(fsm.History()[0].from, "unknown");
    EXPECT_EQ(fsm.History()[0].to, "dark_screen");
    EXPECT_EQ(fsm.History()[1].from, "dark_screen");
    EXPECT_EQ(fsm.History()[1].to, "bright_screen");
}

TEST(ConfigDrivenFSM, IsStuckWhenExceedingMaxDuration)
{
    auto profile = CreateTestProfile();
    // Set very short max duration for testing
    profile.states[0].maxDurationS = 0; // 0 seconds = immediately stuck
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto darkRoi = CreateDarkROI();
    fsm.Update(darkRoi);
    fsm.Update(darkRoi); // -> dark_screen

    // Should be stuck immediately since maxDurationS = 0
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(fsm.IsStuck());
}

TEST(ConfigDrivenFSM, TimeInCurrentStateIncreases)
{
    auto profile = CreateTestProfile();
    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    auto t1 = fsm.TimeInCurrentState();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto t2 = fsm.TimeInCurrentState();
    EXPECT_GT(t2.count(), t1.count());
}

TEST(ConfigDrivenFSM, ReachabilityFilterBlocksUnreachableState)
{
    SH3DS::Core::GameProfile profile;
    profile.gameId = "test_reachability";
    profile.initialState = "state_a";
    profile.debounceFrames = 2;

    // state_a: detects red pixels, transitions to state_b only
    SH3DS::Core::StateDefinition stateA;
    stateA.id = "state_a";
    stateA.detection.method = "color_histogram";
    stateA.detection.roi = "full_screen";
    stateA.detection.hsvLower = cv::Scalar(0, 200, 200);
    stateA.detection.hsvUpper = cv::Scalar(10, 255, 255);
    stateA.detection.pixelRatioMin = 0.5;
    stateA.detection.threshold = 0.5;
    stateA.transitionsTo = {"state_b"};
    profile.states.push_back(stateA);

    // state_b: detects green pixels, transitions to state_c
    SH3DS::Core::StateDefinition stateB;
    stateB.id = "state_b";
    stateB.detection.method = "color_histogram";
    stateB.detection.roi = "full_screen";
    stateB.detection.hsvLower = cv::Scalar(55, 200, 200);
    stateB.detection.hsvUpper = cv::Scalar(65, 255, 255);
    stateB.detection.pixelRatioMin = 0.5;
    stateB.detection.threshold = 0.5;
    stateB.transitionsTo = {"state_c"};
    profile.states.push_back(stateB);

    // state_c: detects blue pixels (high confidence on our test frame)
    SH3DS::Core::StateDefinition stateC;
    stateC.id = "state_c";
    stateC.detection.method = "color_histogram";
    stateC.detection.roi = "full_screen";
    stateC.detection.hsvLower = cv::Scalar(110, 200, 200);
    stateC.detection.hsvUpper = cv::Scalar(130, 255, 255);
    stateC.detection.pixelRatioMin = 0.5;
    stateC.detection.threshold = 0.5;
    stateC.transitionsTo = {"state_a"};
    profile.states.push_back(stateC);

    SH3DS::FSM::ConfigDrivenFSM fsm(profile);
    EXPECT_EQ(fsm.CurrentState(), "state_a");

    // Feed a blue frame: state_c would match with highest confidence,
    // but state_a can only transition to state_b, so state_c is filtered out.
    cv::Mat hsvBlue(240, 400, CV_8UC3, cv::Scalar(120, 255, 255));
    cv::Mat bgrBlue;
    cv::cvtColor(hsvBlue, bgrBlue, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet blueRoi;
    blueRoi["full_screen"] = bgrBlue;

    fsm.Update(blueRoi);
    fsm.Update(blueRoi);
    // Without reachability filter, FSM would try to transition to state_c.
    // With the filter, state_c is not a candidate, so no transition happens.
    EXPECT_EQ(fsm.CurrentState(), "state_a");
}

TEST(ConfigDrivenFSM, ReachabilityFilterAllowsLegalTransition)
{
    SH3DS::Core::GameProfile profile;
    profile.gameId = "test_reachability_legal";
    profile.initialState = "state_a";
    profile.debounceFrames = 2;

    SH3DS::Core::StateDefinition stateA;
    stateA.id = "state_a";
    stateA.detection.method = "color_histogram";
    stateA.detection.roi = "full_screen";
    stateA.detection.hsvLower = cv::Scalar(0, 200, 200);
    stateA.detection.hsvUpper = cv::Scalar(10, 255, 255);
    stateA.detection.pixelRatioMin = 0.5;
    stateA.detection.threshold = 0.5;
    stateA.transitionsTo = {"state_b"};
    profile.states.push_back(stateA);

    SH3DS::Core::StateDefinition stateB;
    stateB.id = "state_b";
    stateB.detection.method = "color_histogram";
    stateB.detection.roi = "full_screen";
    stateB.detection.hsvLower = cv::Scalar(55, 200, 200);
    stateB.detection.hsvUpper = cv::Scalar(65, 255, 255);
    stateB.detection.pixelRatioMin = 0.5;
    stateB.detection.threshold = 0.5;
    stateB.transitionsTo = {};
    profile.states.push_back(stateB);

    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    // Feed green frames — state_b IS reachable from state_a
    cv::Mat hsvGreen(240, 400, CV_8UC3, cv::Scalar(60, 255, 255));
    cv::Mat bgrGreen;
    cv::cvtColor(hsvGreen, bgrGreen, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet greenRoi;
    greenRoi["full_screen"] = bgrGreen;

    fsm.Update(greenRoi);
    auto t = fsm.Update(greenRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "state_b");
}

TEST(ConfigDrivenFSM, IllegalTransitionResetsPendingState)
{
    SH3DS::Core::GameProfile profile;
    profile.gameId = "test_illegal_reset";
    profile.initialState = "state_a";
    profile.debounceFrames = 2;

    // state_a detects red, can transition to state_b only
    SH3DS::Core::StateDefinition stateA;
    stateA.id = "state_a";
    stateA.detection.method = "color_histogram";
    stateA.detection.roi = "full_screen";
    stateA.detection.hsvLower = cv::Scalar(0, 200, 200);
    stateA.detection.hsvUpper = cv::Scalar(10, 255, 255);
    stateA.detection.pixelRatioMin = 0.5;
    stateA.detection.threshold = 0.5;
    stateA.transitionsTo = {"state_b"};
    profile.states.push_back(stateA);

    // state_b detects green
    SH3DS::Core::StateDefinition stateB;
    stateB.id = "state_b";
    stateB.detection.method = "color_histogram";
    stateB.detection.roi = "full_screen";
    stateB.detection.hsvLower = cv::Scalar(55, 200, 200);
    stateB.detection.hsvUpper = cv::Scalar(65, 255, 255);
    stateB.detection.pixelRatioMin = 0.5;
    stateB.detection.threshold = 0.5;
    stateB.transitionsTo = {};
    profile.states.push_back(stateB);

    SH3DS::FSM::ConfigDrivenFSM fsm(profile);

    // With reachability filter, state_b IS in the candidate set, so green frames should transition
    cv::Mat hsvGreen(240, 400, CV_8UC3, cv::Scalar(60, 255, 255));
    cv::Mat bgrGreen;
    cv::cvtColor(hsvGreen, bgrGreen, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet greenRoi;
    greenRoi["full_screen"] = bgrGreen;

    // After debounce, should transition to state_b (legal from state_a)
    fsm.Update(greenRoi);
    auto t = fsm.Update(greenRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->from, "state_a");
    EXPECT_EQ(t->to, "state_b");
}
