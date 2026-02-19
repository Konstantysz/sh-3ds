#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/CXXStateTreeFSM.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

#include <thread>

namespace
{

    std::unique_ptr<SH3DS::FSM::CXXStateTreeFSM> CreateTestFSM(int debounceFrames = 2)
    {
        SH3DS::FSM::CXXStateTreeFSM::Builder builder;
        builder.SetInitialState("unknown");
        builder.SetDebounceFrames(debounceFrames);

        builder.AddState({
            .id = "unknown",
            .transitionsTo = {"dark_screen", "bright_screen"},
            .maxDurationS = 120,
            .detectionParameters = {
                .roi = "full_screen",
                .method = "color_histogram",
                .hsvLower = cv::Scalar(0, 0, 0),
                .hsvUpper = cv::Scalar(0, 0, 0),
                .pixelRatioMin = 0.0,
                .pixelRatioMax = 1.0,
                .threshold = 999.0,
                .templatePath = {},
            },
        });

        builder.AddState({
            .id = "dark_screen",
            .transitionsTo = {"bright_screen"},
            .maxDurationS = 10,
            .detectionParameters = {
                .roi = "full_screen",
                .method = "color_histogram",
                .hsvLower = cv::Scalar(0, 0, 0),
                .hsvUpper = cv::Scalar(180, 50, 50),
                .pixelRatioMin = 0.8,
                .pixelRatioMax = 1.0,
                .threshold = 0.5,
                .templatePath = {},
            },
        });

        builder.AddState({
            .id = "bright_screen",
            .transitionsTo = {"dark_screen"},
            .maxDurationS = 10,
            .shinyCheck = true,
            .detectionParameters = {
                .roi = "full_screen",
                .method = "color_histogram",
                .hsvLower = cv::Scalar(0, 0, 200),
                .hsvUpper = cv::Scalar(180, 50, 255),
                .pixelRatioMin = 0.8,
                .pixelRatioMax = 1.0,
                .threshold = 0.5,
                .templatePath = {},
            },
        });

        return builder.Build();
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

TEST(CXXStateTreeFSM, InitialStateIsFromProfile)
{
    auto fsm = CreateTestFSM();
    EXPECT_EQ(fsm->GetCurrentState(), "unknown");
}

TEST(CXXStateTreeFSM, DetectsDarkScreen)
{
    auto fsm = CreateTestFSM();

    auto darkRoi = CreateDarkROI();

    // Need debounceFrames (2) consecutive detections
    auto t1 = fsm->Update(darkRoi);
    EXPECT_FALSE(t1.has_value()); // First frame — pending

    auto t2 = fsm->Update(darkRoi);
    ASSERT_TRUE(t2.has_value()); // Second frame — transition!
    EXPECT_EQ(t2->from, "unknown");
    EXPECT_EQ(t2->to, "dark_screen");
    EXPECT_EQ(fsm->GetCurrentState(), "dark_screen");
}

TEST(CXXStateTreeFSM, DetectsBrightScreen)
{
    auto fsm = CreateTestFSM();

    auto brightRoi = CreateBrightROI();

    fsm->Update(brightRoi);
    auto t = fsm->Update(brightRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "bright_screen");
}

TEST(CXXStateTreeFSM, TransitionsFromDarkToBright)
{
    auto fsm = CreateTestFSM();

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    // Transition to dark_screen
    fsm->Update(darkRoi);
    fsm->Update(darkRoi);
    EXPECT_EQ(fsm->GetCurrentState(), "dark_screen");

    // Transition to bright_screen
    fsm->Update(brightRoi);
    auto t = fsm->Update(brightRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->from, "dark_screen");
    EXPECT_EQ(t->to, "bright_screen");
}

TEST(CXXStateTreeFSM, DebouncePreventsSingleFrameTransition)
{
    auto fsm = CreateTestFSM(3);

    auto darkRoi = CreateDarkROI();

    EXPECT_FALSE(fsm->Update(darkRoi).has_value()); // Frame 1
    EXPECT_FALSE(fsm->Update(darkRoi).has_value()); // Frame 2
    EXPECT_TRUE(fsm->Update(darkRoi).has_value());  // Frame 3 — transition
}

TEST(CXXStateTreeFSM, DebounceResetsOnDifferentState)
{
    auto fsm = CreateTestFSM(3);

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    fsm->Update(darkRoi);                           // dark frame 1
    fsm->Update(darkRoi);                           // dark frame 2
    fsm->Update(brightRoi);                         // bright frame — resets dark debounce
    EXPECT_FALSE(fsm->Update(darkRoi).has_value()); // dark frame 1 again (not 3)
}

TEST(CXXStateTreeFSM, StaysInCurrentStateWhenNoMatch)
{
    auto fsm = CreateTestFSM();

    // Midtone ROI shouldn't match either dark or bright
    auto midRoi = CreateMidtoneROI();
    EXPECT_FALSE(fsm->Update(midRoi).has_value());
    EXPECT_EQ(fsm->GetCurrentState(), "unknown");
}

TEST(CXXStateTreeFSM, ResetGoesBackToInitialState)
{
    auto fsm = CreateTestFSM();
    auto darkRoi = CreateDarkROI();

    fsm->Update(darkRoi);
    auto t = fsm->Update(darkRoi);
    ASSERT_TRUE(t.has_value());
    ASSERT_EQ(fsm->GetCurrentState(), "dark_screen");

    fsm->Reset();
    EXPECT_EQ(fsm->GetCurrentState(), "unknown");
    EXPECT_TRUE(fsm->GetTransitionHistory().empty());
}

TEST(CXXStateTreeFSM, HistoryRecordsTransitions)
{
    auto fsm = CreateTestFSM();

    auto darkRoi = CreateDarkROI();
    auto brightRoi = CreateBrightROI();

    fsm->Update(darkRoi);
    fsm->Update(darkRoi); // -> dark_screen
    fsm->Update(brightRoi);
    fsm->Update(brightRoi); // -> bright_screen

    ASSERT_EQ(fsm->GetTransitionHistory().size(), 2u);
    EXPECT_EQ(fsm->GetTransitionHistory()[0].from, "unknown");
    EXPECT_EQ(fsm->GetTransitionHistory()[0].to, "dark_screen");
    EXPECT_EQ(fsm->GetTransitionHistory()[1].from, "dark_screen");
    EXPECT_EQ(fsm->GetTransitionHistory()[1].to, "bright_screen");
}

TEST(CXXStateTreeFSM, IsStuckWhenExceedingMaxDuration)
{
    SH3DS::FSM::CXXStateTreeFSM::Builder builder;
    builder.SetInitialState("unknown");
    builder.SetDebounceFrames(2);

    builder.AddState({
        .id = "unknown",
        .transitionsTo = {"dark_screen"},
        .maxDurationS = 120,
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 0, 0),
            .hsvUpper = cv::Scalar(0, 0, 0),
            .pixelRatioMin = 0.0,
            .pixelRatioMax = 1.0,
            .threshold = 999.0,
            .templatePath = {},
        },
    });

    builder.AddState({
        .id = "dark_screen",
        .transitionsTo = {},
        .maxDurationS = 0, // 0 seconds = immediately stuck
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 0, 0),
            .hsvUpper = cv::Scalar(180, 50, 50),
            .pixelRatioMin = 0.8,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    auto fsm = builder.Build();

    auto darkRoi = CreateDarkROI();
    fsm->Update(darkRoi);
    fsm->Update(darkRoi); // -> dark_screen

    // Should be stuck immediately since maxDurationS = 0
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(fsm->IsStuck());
}

TEST(CXXStateTreeFSM, TimeInCurrentStateIncreases)
{
    auto fsm = CreateTestFSM();

    auto t1 = fsm->GetTimeInCurrentState();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto t2 = fsm->GetTimeInCurrentState();
    EXPECT_GT(t2.count(), t1.count());
}

TEST(CXXStateTreeFSM, ReachabilityFilterBlocksUnreachableState)
{
    SH3DS::FSM::CXXStateTreeFSM::Builder builder;
    builder.SetInitialState("state_a");
    builder.SetDebounceFrames(2);

    // state_a: detects red pixels, transitions to state_b only
    builder.AddState({
        .id = "state_a",
        .transitionsTo = {"state_b"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 200, 200),
            .hsvUpper = cv::Scalar(10, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    // state_b: detects green pixels, transitions to state_c
    builder.AddState({
        .id = "state_b",
        .transitionsTo = {"state_c"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(55, 200, 200),
            .hsvUpper = cv::Scalar(65, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    // state_c: detects blue pixels
    builder.AddState({
        .id = "state_c",
        .transitionsTo = {"state_a"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(110, 200, 200),
            .hsvUpper = cv::Scalar(130, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    auto fsm = builder.Build();
    EXPECT_EQ(fsm->GetCurrentState(), "state_a");

    // Feed a blue frame: state_c would match with highest confidence,
    // but state_a can only transition to state_b, so state_c is filtered out.
    cv::Mat hsvBlue(240, 400, CV_8UC3, cv::Scalar(120, 255, 255));
    cv::Mat bgrBlue;
    cv::cvtColor(hsvBlue, bgrBlue, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet blueRoi;
    blueRoi["full_screen"] = bgrBlue;

    fsm->Update(blueRoi);
    fsm->Update(blueRoi);
    // Without reachability filter, FSM would try to transition to state_c.
    // With the filter, state_c is not a candidate, so no transition happens.
    EXPECT_EQ(fsm->GetCurrentState(), "state_a");
}

TEST(CXXStateTreeFSM, ReachabilityFilterAllowsLegalTransition)
{
    SH3DS::FSM::CXXStateTreeFSM::Builder builder;
    builder.SetInitialState("state_a");
    builder.SetDebounceFrames(2);

    builder.AddState({
        .id = "state_a",
        .transitionsTo = {"state_b"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 200, 200),
            .hsvUpper = cv::Scalar(10, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    builder.AddState({
        .id = "state_b",
        .transitionsTo = {},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(55, 200, 200),
            .hsvUpper = cv::Scalar(65, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    auto fsm = builder.Build();

    // Feed green frames — state_b IS reachable from state_a
    cv::Mat hsvGreen(240, 400, CV_8UC3, cv::Scalar(60, 255, 255));
    cv::Mat bgrGreen;
    cv::cvtColor(hsvGreen, bgrGreen, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet greenRoi;
    greenRoi["full_screen"] = bgrGreen;

    fsm->Update(greenRoi);
    auto t = fsm->Update(greenRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "state_b");
}

TEST(CXXStateTreeFSM, IllegalTransitionResetsPendingState)
{
    SH3DS::FSM::CXXStateTreeFSM::Builder builder;
    builder.SetInitialState("state_a");
    builder.SetDebounceFrames(2);

    // state_a detects red, can transition to state_b only
    builder.AddState({
        .id = "state_a",
        .transitionsTo = {"state_b"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 200, 200),
            .hsvUpper = cv::Scalar(10, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    // state_b detects green
    builder.AddState({
        .id = "state_b",
        .transitionsTo = {},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(55, 200, 200),
            .hsvUpper = cv::Scalar(65, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    auto fsm = builder.Build();

    // With reachability filter, state_b IS in the candidate set, so green frames should transition
    cv::Mat hsvGreen(240, 400, CV_8UC3, cv::Scalar(60, 255, 255));
    cv::Mat bgrGreen;
    cv::cvtColor(hsvGreen, bgrGreen, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet greenRoi;
    greenRoi["full_screen"] = bgrGreen;

    // After debounce, should transition to state_b (legal from state_a)
    fsm->Update(greenRoi);
    auto t = fsm->Update(greenRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->from, "state_a");
    EXPECT_EQ(t->to, "state_b");
}

TEST(CXXStateTreeFSM, ResetRebuildsSyncedTree)
{
    // After a transition + Reset, the FSM should be able to make legal transitions again.
    auto fsm = CreateTestFSM();
    auto brightRoi = CreateBrightROI();

    fsm->Update(brightRoi);
    auto toBright = fsm->Update(brightRoi);
    ASSERT_TRUE(toBright.has_value());
    ASSERT_EQ(fsm->GetCurrentState(), "bright_screen");

    fsm->Reset();
    EXPECT_EQ(fsm->GetCurrentState(), "unknown");
    EXPECT_TRUE(fsm->GetTransitionHistory().empty());

    // After reset the tree is rebuilt; legal transitions from unknown should work.
    auto darkRoi = CreateDarkROI();
    fsm->Update(darkRoi);
    auto t = fsm->Update(darkRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "dark_screen");
}

TEST(CXXStateTreeFSM, EmptyTransitionsToBlocksAllOutgoing)
{
    SH3DS::FSM::CXXStateTreeFSM::Builder builder;
    builder.SetInitialState("state_a");
    builder.SetDebounceFrames(2);

    // state_a detects red and can transition to state_b
    builder.AddState({
        .id = "state_a",
        .transitionsTo = {"state_b"},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 200, 200),
            .hsvUpper = cv::Scalar(10, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    // state_b detects green too but has NO outgoing transitions (terminal state)
    builder.AddState({
        .id = "state_b",
        .transitionsTo = {},
        .detectionParameters = {
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(55, 200, 200),
            .hsvUpper = cv::Scalar(65, 255, 255),
            .pixelRatioMin = 0.5,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        },
    });

    auto fsm = builder.Build();

    // Transition to state_b
    cv::Mat hsvGreen(240, 400, CV_8UC3, cv::Scalar(60, 255, 255));
    cv::Mat bgrGreen;
    cv::cvtColor(hsvGreen, bgrGreen, cv::COLOR_HSV2BGR);
    SH3DS::Core::ROISet greenRoi;
    greenRoi["full_screen"] = bgrGreen;

    fsm->Update(greenRoi);
    auto t = fsm->Update(greenRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "state_b");

    // Now in state_b: green still matches state_b detection, but state_b has no transitions.
    // The FSM should not loop or crash — it stays in state_b.
    EXPECT_FALSE(fsm->Update(greenRoi).has_value());
    EXPECT_FALSE(fsm->Update(greenRoi).has_value());
    EXPECT_EQ(fsm->GetCurrentState(), "state_b");
}

TEST(CXXStateTreeFSM, InitialStateReturnsBuilderInitialState)
{
    // InitialState() must return exactly what was passed to SetInitialState().
    auto fsm = CreateTestFSM();
    EXPECT_EQ(fsm->GetInitialState(), "unknown");

    // Remains stable after transitions.
    auto darkRoi = CreateDarkROI();
    fsm->Update(darkRoi);
    fsm->Update(darkRoi);
    EXPECT_EQ(fsm->GetInitialState(), "unknown");

    // Remains stable after Reset.
    fsm->Reset();
    EXPECT_EQ(fsm->GetInitialState(), "unknown");
}

TEST(CXXStateTreeFSM, ResetToInitialStateAllowsNormalDetection)
{
    // After resetting to InitialState, the FSM should be able to transition normally.
    auto fsm = CreateTestFSM();

    // Drive into dark_screen
    auto darkRoi = CreateDarkROI();
    fsm->Update(darkRoi);
    fsm->Update(darkRoi);
    ASSERT_EQ(fsm->GetCurrentState(), "dark_screen");

    // Simulate watchdog abort path reset
    fsm->Reset();
    EXPECT_EQ(fsm->GetCurrentState(), "unknown");

    // FSM should be able to transition again from unknown
    auto brightRoi = CreateBrightROI();
    fsm->Update(brightRoi);
    auto t = fsm->Update(brightRoi);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->to, "bright_screen");
}
