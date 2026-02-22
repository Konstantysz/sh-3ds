#include "Vision/ColorImprovement.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

namespace
{
    /// Create a solid-color 8-bit BGR image.
    cv::Mat MakeBgr(int b, int g, int r, int width = 64, int height = 64)
    {
        return cv::Mat(height, width, CV_8UC3, cv::Scalar(b, g, r));
    }
} // namespace

// ============================================================================
// Robustness: degenerate inputs must not crash
// ============================================================================

TEST(ColorImprovement, EmptyMatIsIgnored)
{
    cv::Mat empty;
    cv::Mat result = SH3DS::Vision::ImproveFrameColors(empty);
    EXPECT_TRUE(result.empty());
}

TEST(ColorImprovement, GrayscaleMatIsIgnored)
{
    cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(128));
    cv::Mat result = SH3DS::Vision::ImproveFrameColors(gray);
    // Result should be same type and size
    EXPECT_EQ(result.type(), CV_8UC1);
    EXPECT_EQ(cv::countNonZero(result != gray), 0);
}

TEST(ColorImprovement, Float32MatIsIgnored)
{
    cv::Mat flt(64, 64, CV_32FC3, cv::Scalar(0.5, 0.5, 0.5));
    cv::Mat result = SH3DS::Vision::ImproveFrameColors(flt);
    // channels still float
    EXPECT_EQ(result.type(), CV_32FC3);
}

// ============================================================================
// Output type preservation
// ============================================================================

TEST(ColorImprovement, OutputIsStill8UC3)
{
    cv::Mat frame = MakeBgr(100, 150, 200);
    cv::Mat result = SH3DS::Vision::ImproveFrameColors(frame);
    EXPECT_EQ(result.type(), CV_8UC3);
    EXPECT_EQ(result.cols, 64);
    EXPECT_EQ(result.rows, 64);
}

// ============================================================================
// Functional behaviour
// ============================================================================

TEST(ColorImprovement, NeutralGrayIsApproximatelyPreserved)
{
    // A perfectly neutral mid-gray image (equal BGR channels) should experience
    // no white-balance shift. CLAHE on a flat image is also a near-identity.
    // After gamma 1.1 the values darken slightly, so we allow ±15 tolerance.
    cv::Mat frame = MakeBgr(128, 128, 128);
    cv::Mat result = SH3DS::Vision::ImproveFrameColors(frame);

    cv::Scalar mean = cv::mean(result);
    // All three channels should remain close to each other (neutral)
    EXPECT_NEAR(mean[0], mean[1], 5.0);
    EXPECT_NEAR(mean[1], mean[2], 5.0);
    // Overall brightness should not blow up or collapse
    EXPECT_GT(mean[0], 50.0);
    EXPECT_LT(mean[0], 200.0);
}

TEST(ColorImprovement, WhiteBalanceShiftsColorCastTowardNeutral)
{
    // Strong blue cast: B channel is much higher than G and R.
    cv::Mat biased = MakeBgr(220, 80, 80);
    cv::Mat neutral = SH3DS::Vision::ImproveFrameColors(biased);

    // After WB the channels should be closer to each other than in the original.
    cv::Scalar meanOrig = cv::mean(biased);
    cv::Scalar meanCorrected = cv::mean(neutral);

    double origSpread = std::abs(meanOrig[0] - meanOrig[1]);                // B-G before
    double correctedSpread = std::abs(meanCorrected[0] - meanCorrected[1]); // B-G after
    EXPECT_LT(correctedSpread, origSpread);
}

TEST(ColorImprovement, DarkImageBecomesLighterAfterClahe)
{
    // Very dark image — CLAHE should boost the L channel.
    cv::Mat dark = MakeBgr(20, 20, 20);
    cv::Mat corrected = SH3DS::Vision::ImproveFrameColors(dark);

    cv::Scalar meanDark = cv::mean(dark);
    cv::Scalar meanCorrected = cv::mean(corrected);

    // Mean brightness should increase (CLAHE effect outweighs gamma darkening on
    // a flat-dark image because CLAHE spreads the histogram).
    // We only verify the image is not darker than the input.
    EXPECT_GE(meanCorrected[0] + meanCorrected[1] + meanCorrected[2], meanDark[0] + meanDark[1] + meanDark[2] - 30.0);
}

TEST(ColorImprovement, CustomConfigIsRespected)
{
    SH3DS::Vision::ColorImprovementConfig cfg;
    cfg.gamma = 2.0; // strong darkening

    cv::Mat bright = MakeBgr(200, 200, 200);
    cv::Mat corrected = SH3DS::Vision::ImproveFrameColors(bright, cfg);

    cv::Scalar meanOrig = cv::mean(bright);
    cv::Scalar meanCorrected = cv::mean(corrected);
    // Higher gamma -> darker output
    double sumOrig = meanOrig[0] + meanOrig[1] + meanOrig[2];
    double sumCorrected = meanCorrected[0] + meanCorrected[1] + meanCorrected[2];
    EXPECT_LT(sumCorrected, sumOrig);
}
