#include "Capture/FramePreprocessor.h"
#include "Core/Config.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

namespace
{

    class DualScreenTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Create a synthetic 640x480 camera frame with a bright quad for the "screen"
            cameraFrame = cv::Mat(480, 640, CV_8UC3, cv::Scalar(30, 30, 30));

            topCorners = {
                cv::Point2f(100.0f, 50.0f),
                cv::Point2f(540.0f, 50.0f),
                cv::Point2f(540.0f, 200.0f),
                cv::Point2f(100.0f, 200.0f),
            };

            bottomCorners = {
                cv::Point2f(120.0f, 220.0f),
                cv::Point2f(520.0f, 220.0f),
                cv::Point2f(520.0f, 420.0f),
                cv::Point2f(120.0f, 420.0f),
            };

            // Fill top screen area
            std::vector<cv::Point> topPts = {
                cv::Point(100, 50), cv::Point(540, 50), cv::Point(540, 200), cv::Point(100, 200)
            };
            cv::fillConvexPoly(cameraFrame, topPts, cv::Scalar(200, 180, 160));

            // Fill bottom screen area
            std::vector<cv::Point> bottomPts = {
                cv::Point(120, 220), cv::Point(520, 220), cv::Point(520, 420), cv::Point(120, 420)
            };
            cv::fillConvexPoly(cameraFrame, bottomPts, cv::Scalar(100, 150, 200));

            topCalib.corners = topCorners;
            topCalib.targetWidth = 400;
            topCalib.targetHeight = 240;

            bottomCalib.corners = bottomCorners;
            bottomCalib.targetWidth = 320;
            bottomCalib.targetHeight = 240;
        }

        cv::Mat cameraFrame;
        std::array<cv::Point2f, 4> topCorners;
        std::array<cv::Point2f, 4> bottomCorners;
        SH3DS::Core::ScreenCalibrationConfig topCalib;
        SH3DS::Core::ScreenCalibrationConfig bottomCalib;
    };

} // namespace

TEST_F(DualScreenTest, ProcessDualScreenReturnsBothScreens)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, bottomCalib);
    auto result = preprocessor.ProcessDualScreen(cameraFrame);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->warpedTop.empty());
    EXPECT_EQ(result->warpedTop.cols, 400);
    EXPECT_EQ(result->warpedTop.rows, 240);
    EXPECT_FALSE(result->warpedBottom.empty());
    EXPECT_EQ(result->warpedBottom.cols, 320);
    EXPECT_EQ(result->warpedBottom.rows, 240);
}

TEST_F(DualScreenTest, ProcessDualScreenExtractsTopROIs)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "top_full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
        { .name = "top_strip", .x = 0.1, .y = 0.05, .w = 0.8, .h = 0.12 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, bottomCalib);
    auto result = preprocessor.ProcessDualScreen(cameraFrame);

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->topRois.empty());
    EXPECT_TRUE(result->topRois.contains("top_full"));
    EXPECT_TRUE(result->topRois.contains("top_strip"));
    ASSERT_FALSE(result->bottomRois.empty());
    EXPECT_TRUE(result->bottomRois.contains("top_full"));
    EXPECT_TRUE(result->bottomRois.contains("top_strip"));
}

TEST_F(DualScreenTest, ProcessDualScreenWithoutBottomCalibration)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, std::nullopt);
    auto result = preprocessor.ProcessDualScreen(cameraFrame);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->warpedTop.empty());
    EXPECT_TRUE(result->warpedBottom.empty());
}

TEST_F(DualScreenTest, ProcessDualScreenEmptyFrameReturnsNullopt)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, bottomCalib);
    auto result = preprocessor.ProcessDualScreen(cv::Mat());

    EXPECT_FALSE(result.has_value());
}

TEST_F(DualScreenTest, DegenerateCornersDisablesBottomScreen)
{
    SH3DS::Core::ScreenCalibrationConfig degenerate;
    degenerate.corners = {
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(0.0f, 0.0f),
    };
    degenerate.targetWidth = 320;
    degenerate.targetHeight = 240;

    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, degenerate);
    auto result = preprocessor.ProcessDualScreen(cameraFrame);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->warpedTop.empty());
    // Degenerate corners should have been disabled, so bottom is empty
    EXPECT_TRUE(result->warpedBottom.empty());
}

TEST_F(DualScreenTest, ProcessAndProcessDualScreenProduceSameTopROIs)
{
    std::vector<SH3DS::Core::RoiDefinition> rois = {
        { .name = "full", .x = 0.0, .y = 0.0, .w = 1.0, .h = 1.0 },
    };

    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, rois, std::nullopt);

    auto singleResult = preprocessor.Process(cameraFrame);
    auto dualResult = preprocessor.ProcessDualScreen(cameraFrame);

    ASSERT_TRUE(singleResult.has_value());
    ASSERT_TRUE(dualResult.has_value());
    ASSERT_FALSE(dualResult->topRois.empty());

    auto &singleRoi = singleResult->at("full");
    auto &dualRoi = dualResult->topRois.at("full");

    EXPECT_EQ(singleRoi.cols, dualRoi.cols);
    EXPECT_EQ(singleRoi.rows, dualRoi.rows);
}


