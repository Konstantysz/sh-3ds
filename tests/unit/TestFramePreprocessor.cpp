#include "Capture/FramePreprocessor.h"
#include "Core/Config.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

class FramePreprocessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cameraFrame = cv::Mat(480, 640, CV_8UC3, cv::Scalar(30, 30, 30));

        screenCorners = {
            cv::Point2f(100.0f, 50.0f),
            cv::Point2f(540.0f, 50.0f),
            cv::Point2f(540.0f, 330.0f),
            cv::Point2f(100.0f, 330.0f),
        };

        std::vector<cv::Point> pts = {
            cv::Point(100, 50), cv::Point(540, 50), cv::Point(540, 330), cv::Point(100, 330)
        };
        cv::fillConvexPoly(cameraFrame, pts, cv::Scalar(200, 180, 160));

        calibration.corners = screenCorners;
        calibration.targetWidth = 400;
        calibration.targetHeight = 240;
    }

    cv::Mat cameraFrame;
    std::array<cv::Point2f, 4> screenCorners;
    SH3DS::Core::ScreenCalibrationConfig calibration;
};

TEST_F(FramePreprocessorTest, ProcessReturnsROISet)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full_screen", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result = preprocessor.Process(cameraFrame);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->contains("full_screen"));
}

TEST_F(FramePreprocessorTest, WarpedScreenHasCorrectDimensions)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full_screen", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result = preprocessor.Process(cameraFrame);

    ASSERT_TRUE(result.has_value());
    auto it = result->find("full_screen");
    ASSERT_NE(it, result->end());
    EXPECT_EQ(it->second.cols, 400);
    EXPECT_EQ(it->second.rows, 240);
}

TEST_F(FramePreprocessorTest, ExtractsMultipleROIs)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "pokemon_sprite", .x = 0.30, .y = 0.05, .w = 0.40, .h = 0.65 },
        { .name = "dialogue_box", .x = 0.02, .y = 0.72, .w = 0.96, .h = 0.26 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result = preprocessor.Process(cameraFrame);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->contains("pokemon_sprite"));
    EXPECT_TRUE(result->contains("dialogue_box"));

    auto it = result->find("pokemon_sprite");
    ASSERT_NE(it, result->end());
    EXPECT_EQ(it->second.cols, 160);
    EXPECT_EQ(it->second.rows, 156);
}

TEST_F(FramePreprocessorTest, ProcessReturnsNulloptForEmptyFrame)
{
    cv::Mat empty;
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full_screen", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result = preprocessor.Process(empty);

    EXPECT_FALSE(result.has_value());
}

TEST_F(FramePreprocessorTest, SetFixedCornersUpdatesWarp)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full_screen", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result1 = preprocessor.Process(cameraFrame);
    ASSERT_TRUE(result1.has_value());

    std::array<cv::Point2f, 4> newCorners = {
        cv::Point2f(50.0f, 25.0f),
        cv::Point2f(590.0f, 25.0f),
        cv::Point2f(590.0f, 455.0f),
        cv::Point2f(50.0f, 455.0f),
    };
    preprocessor.SetFixedCorners(newCorners);

    auto result2 = preprocessor.Process(cameraFrame);
    ASSERT_TRUE(result2.has_value());
}

TEST_F(FramePreprocessorTest, ROIClampsToImageBounds)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "oversized", .x = 0.9, .y = 0.9, .w = 0.5, .h = 0.5 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(calibration, rois);
    auto result = preprocessor.Process(cameraFrame);

    ASSERT_TRUE(result.has_value());
    auto it = result->find("oversized");
    ASSERT_NE(it, result->end());
    EXPECT_GT(it->second.cols, 0);
    EXPECT_GT(it->second.rows, 0);
}
