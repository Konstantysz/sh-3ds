#include "Capture/FramePreprocessor.h"
#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/ConfigDrivenFSM.h"
#include "Vision/DominantColorDetector.h"
#include "Vision/ShinyDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

/// Integration test: simulates a complete SR cycle with synthetic frames.
/// Tests the pipeline: FramePreprocessor -> ConfigDrivenFSM -> ShinyDetector.
class ReplayPipelineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        calibration.corners = {
            cv::Point2f(0.0f, 0.0f),
            cv::Point2f(400.0f, 0.0f),
            cv::Point2f(400.0f, 240.0f),
            cv::Point2f(0.0f, 240.0f),
        };
        calibration.targetWidth = 400;
        calibration.targetHeight = 240;

        rois.push_back({ .name = "full_screen", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 });
        rois.push_back({ .name = "pokemon_sprite", .x = 0.3, .y = 0.05, .w = 0.4, .h = 0.65 });

        preprocessor = std::make_unique<SH3DS::Capture::FramePreprocessor>(calibration, rois);

        // Game profile with 2 states
        SH3DS::Core::GameProfile profile;
        profile.gameId = "test";
        profile.initialState = "unknown";
        profile.debounceFrames = 2;

        SH3DS::Core::StateDefinition darkState;
        darkState.id = "dark_screen";
        darkState.detection.method = "color_histogram";
        darkState.detection.roi = "full_screen";
        darkState.detection.hsvLower = cv::Scalar(0, 0, 0);
        darkState.detection.hsvUpper = cv::Scalar(180, 50, 50);
        darkState.detection.pixelRatioMin = 0.7;
        darkState.detection.pixelRatioMax = 1.0;
        darkState.detection.threshold = 0.5;
        darkState.maxDurationS = 30;
        profile.states.push_back(darkState);

        SH3DS::Core::StateDefinition brightState;
        brightState.id = "bright_screen";
        brightState.detection.method = "color_histogram";
        brightState.detection.roi = "full_screen";
        brightState.detection.hsvLower = cv::Scalar(0, 0, 200);
        brightState.detection.hsvUpper = cv::Scalar(180, 50, 255);
        brightState.detection.pixelRatioMin = 0.7;
        brightState.detection.pixelRatioMax = 1.0;
        brightState.detection.threshold = 0.5;
        brightState.maxDurationS = 30;
        brightState.shinyCheck = true;
        profile.states.push_back(brightState);

        fsm = std::make_unique<SH3DS::FSM::ConfigDrivenFSM>(profile);

        // Detector
        SH3DS::Core::DetectionMethodConfig detConfig;
        detConfig.method = "dominant_color";
        detConfig.normalHsvLower = cv::Scalar(100, 100, 60);
        detConfig.normalHsvUpper = cv::Scalar(130, 255, 200);
        detConfig.shinyHsvLower = cv::Scalar(95, 25, 170);
        detConfig.shinyHsvUpper = cv::Scalar(135, 110, 255);
        detConfig.shinyRatioThreshold = 0.12;
        detConfig.normalRatioThreshold = 0.12;
        detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(detConfig, "test");
    }

    /// Create a synthetic "camera frame" with a specific color (already 400x240).
    cv::Mat MakeFrame(cv::Scalar bgrColor)
    {
        return cv::Mat(240, 400, CV_8UC3, bgrColor);
    }

    SH3DS::Core::ScreenCalibrationConfig calibration;
    std::vector<SH3DS::Core::RoiDefinition> rois;
    std::unique_ptr<SH3DS::Capture::FramePreprocessor> preprocessor;
    std::unique_ptr<SH3DS::FSM::ConfigDrivenFSM> fsm;
    std::unique_ptr<SH3DS::Vision::ShinyDetector> detector;
};

TEST_F(ReplayPipelineTest, FullPipelineDarkToBrightTransition)
{
    // Simulate dark frames (soft reset)
    for (int i = 0; i < 3; ++i)
    {
        auto frame = MakeFrame(cv::Scalar(10, 10, 10));
        auto roiSet = preprocessor->Process(frame);
        ASSERT_TRUE(roiSet.has_value());
        fsm->Update(*roiSet);
    }
    EXPECT_EQ(fsm->CurrentState(), "dark_screen");

    // Simulate bright frames (title screen / reveal)
    for (int i = 0; i < 3; ++i)
    {
        auto frame = MakeFrame(cv::Scalar(240, 240, 240));
        auto roiSet = preprocessor->Process(frame);
        ASSERT_TRUE(roiSet.has_value());
        fsm->Update(*roiSet);
    }
    EXPECT_EQ(fsm->CurrentState(), "bright_screen");

    // Verify history
    ASSERT_GE(fsm->History().size(), 2u);
    EXPECT_EQ(fsm->History()[0].to, "dark_screen");
    EXPECT_EQ(fsm->History()[1].to, "bright_screen");
}

TEST_F(ReplayPipelineTest, ShinyDetectorIntegration)
{
    // Create a normal Froakie colored frame (dark blue in HSV: H=115, S=180, V=130)
    cv::Mat hsvNormal(240, 400, CV_8UC3, cv::Scalar(115, 180, 130));
    cv::Mat bgrNormal;
    cv::cvtColor(hsvNormal, bgrNormal, cv::COLOR_HSV2BGR);

    auto roiSet = preprocessor->Process(bgrNormal);
    ASSERT_TRUE(roiSet.has_value());

    auto it = roiSet->find("pokemon_sprite");
    ASSERT_NE(it, roiSet->end());

    auto result = detector->Detect(it->second);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::NotShiny);
}

TEST_F(ReplayPipelineTest, ShinyDetectorFindsShinySpriteInPipeline)
{
    // Create a shiny Froakie colored frame (pastel blue: H=115, S=60, V=220)
    cv::Mat hsvShiny(240, 400, CV_8UC3, cv::Scalar(115, 60, 220));
    cv::Mat bgrShiny;
    cv::cvtColor(hsvShiny, bgrShiny, cv::COLOR_HSV2BGR);

    auto roiSet = preprocessor->Process(bgrShiny);
    ASSERT_TRUE(roiSet.has_value());

    auto it = roiSet->find("pokemon_sprite");
    ASSERT_NE(it, roiSet->end());

    auto result = detector->Detect(it->second);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Shiny);
}
