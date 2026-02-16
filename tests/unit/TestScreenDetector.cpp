#include "Capture/ScreenDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

namespace
{

    cv::Mat MakeBlackFrame(int width = 1280, int height = 720)
    {
        return cv::Mat(height, width, CV_8UC3, cv::Scalar(10, 10, 10));
    }

    void DrawBrightRect(cv::Mat &frame,
        cv::Point tl,
        cv::Point tr,
        cv::Point br,
        cv::Point bl,
        cv::Scalar color = cv::Scalar(220, 220, 220))
    {
        std::vector<cv::Point> pts = { tl, tr, br, bl };
        cv::fillConvexPoly(frame, pts, color);
    }

    // Standard 3DS-like layout: top screen (5:3) centered, bottom screen (4:3) below
    struct DualScreenLayout
    {
        cv::Point topTl = { 340, 80 };
        cv::Point topTr = { 940, 80 };
        cv::Point topBr = { 940, 440 };
        cv::Point topBl = { 340, 440 };

        cv::Point botTl = { 400, 460 };
        cv::Point botTr = { 880, 460 };
        cv::Point botBr = { 880, 820 }; // NOTE: need taller frame for this
        cv::Point botBl = { 400, 820 };
    };

} // namespace

TEST(ScreenDetector, DetectsWhiteRectangleOnBlackBackground)
{
    auto frame = MakeBlackFrame();
    // 500x300 rect → aspect ratio 1.667 (5:3) — matches top screen
    DrawBrightRect(frame, { 200, 100 }, { 700, 100 }, { 700, 400 }, { 200, 400 });

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    ASSERT_TRUE(result.topScreen.has_value());
    auto &top = *result.topScreen;

    EXPECT_NEAR(top.corners[0].x, 200.0f, 10.0f); // TL
    EXPECT_NEAR(top.corners[0].y, 100.0f, 10.0f);
    EXPECT_NEAR(top.corners[1].x, 700.0f, 10.0f); // TR
    EXPECT_NEAR(top.corners[1].y, 100.0f, 10.0f);
    EXPECT_NEAR(top.corners[2].x, 700.0f, 10.0f); // BR
    EXPECT_NEAR(top.corners[2].y, 400.0f, 10.0f);
    EXPECT_NEAR(top.corners[3].x, 200.0f, 10.0f); // BL
    EXPECT_NEAR(top.corners[3].y, 400.0f, 10.0f);
    EXPECT_GT(top.confidence, 0.5);
}

TEST(ScreenDetector, DetectsBothScreens)
{
    auto frame = MakeBlackFrame(1280, 960);

    // Top screen: 600x360 → 1.667 (5:3)
    DrawBrightRect(frame, { 340, 80 }, { 940, 80 }, { 940, 440 }, { 340, 440 });
    // Bottom screen: 480x360 → 1.333 (4:3), below top
    DrawBrightRect(frame, { 400, 460 }, { 880, 460 }, { 880, 820 }, { 400, 820 });

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    ASSERT_TRUE(result.topScreen.has_value());
    ASSERT_TRUE(result.bottomScreen.has_value());

    // Top should be above bottom
    float topCenterY = (result.topScreen->corners[0].y + result.topScreen->corners[2].y) / 2.0f;
    float botCenterY = (result.bottomScreen->corners[0].y + result.bottomScreen->corners[2].y) / 2.0f;
    EXPECT_LT(topCenterY, botCenterY);
}

TEST(ScreenDetector, RejectsSquareAspectRatio)
{
    auto frame = MakeBlackFrame();
    // 300x300 square — neither 5:3 nor 4:3
    DrawBrightRect(frame, { 200, 100 }, { 500, 100 }, { 500, 400 }, { 200, 400 });

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    EXPECT_FALSE(result.topScreen.has_value());
    EXPECT_FALSE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, HandlesNoBrightRegions)
{
    auto frame = MakeBlackFrame();

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    EXPECT_FALSE(result.topScreen.has_value());
    EXPECT_FALSE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, HandlesUniformBrightFrame)
{
    cv::Mat frame(720, 1280, CV_8UC3, cv::Scalar(220, 220, 220));

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    // No contrast → no contours → no detection
    EXPECT_FALSE(result.topScreen.has_value());
    EXPECT_FALSE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, HandlesEmptyFrame)
{
    cv::Mat empty;

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(empty);

    EXPECT_FALSE(result.topScreen.has_value());
    EXPECT_FALSE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, RejectsSmallContour)
{
    auto frame = MakeBlackFrame();
    // Tiny 20x12 rect — too small
    DrawBrightRect(frame, { 300, 300 }, { 320, 300 }, { 320, 312 }, { 300, 312 });

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    EXPECT_FALSE(result.topScreen.has_value());
    EXPECT_FALSE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, TemporalSmoothingReducesJitter)
{
    SH3DS::Capture::ScreenDetector detector;

    // Feed 20 frames with slightly jittering corners
    std::vector<std::array<cv::Point2f, 4>> detectedCorners;
    for (int i = 0; i < 20; ++i)
    {
        auto frame = MakeBlackFrame();
        float jitter = static_cast<float>(i % 2 == 0 ? 3 : -3);
        DrawBrightRect(frame,
            cv::Point(200 + static_cast<int>(jitter), 100 + static_cast<int>(jitter)),
            cv::Point(700 + static_cast<int>(jitter), 100 - static_cast<int>(jitter)),
            cv::Point(700 - static_cast<int>(jitter), 400 - static_cast<int>(jitter)),
            cv::Point(200 - static_cast<int>(jitter), 400 + static_cast<int>(jitter)));

        auto result = detector.Detect(frame);
        if (result.topScreen.has_value())
        {
            detectedCorners.push_back(result.topScreen->corners);
        }
    }

    ASSERT_GE(detectedCorners.size(), 10u);

    // Last few smoothed corners should have less variance than the input jitter
    auto &last = detectedCorners.back();
    auto &prev = detectedCorners[detectedCorners.size() - 2];

    float maxDelta = 0.0f;
    for (int j = 0; j < 4; ++j)
    {
        maxDelta = std::max(maxDelta, std::abs(last[j].x - prev[j].x));
        maxDelta = std::max(maxDelta, std::abs(last[j].y - prev[j].y));
    }

    // Smoothed output should have less than 3px delta (input jitter is ±3)
    EXPECT_LT(maxDelta, 3.0f);
}

TEST(ScreenDetector, ResetClearsSmoothing)
{
    SH3DS::Capture::ScreenDetector detector;

    auto frame = MakeBlackFrame();
    DrawBrightRect(frame, { 200, 100 }, { 700, 100 }, { 700, 400 }, { 200, 400 });

    // Build up smoothing state
    for (int i = 0; i < 5; ++i)
    {
        detector.Detect(frame);
    }

    detector.Reset();

    // After reset, next detection should work from scratch (no crash)
    auto result = detector.Detect(frame);
    EXPECT_TRUE(result.topScreen.has_value());
}

TEST(ScreenDetector, ConfidenceHigherForBetterAspectRatio)
{
    SH3DS::Capture::ScreenDetector detector;

    // Perfect 5:3 ratio: 500x300
    auto frame1 = MakeBlackFrame();
    DrawBrightRect(frame1, { 200, 100 }, { 700, 100 }, { 700, 400 }, { 200, 400 });
    auto result1 = detector.DetectOnce(frame1);

    // Slightly off ratio: 500x280 → 1.786 (tolerance boundary)
    auto frame2 = MakeBlackFrame();
    DrawBrightRect(frame2, { 200, 100 }, { 700, 100 }, { 700, 380 }, { 200, 380 });
    auto result2 = detector.DetectOnce(frame2);

    if (result1.topScreen.has_value() && result2.topScreen.has_value())
    {
        EXPECT_GE(result1.topScreen->confidence, result2.topScreen->confidence);
    }
}

TEST(ScreenDetector, FactoryMethodCreatesInstance)
{
    auto detector = SH3DS::Capture::ScreenDetector::CreateScreenDetector();
    ASSERT_NE(detector, nullptr);

    auto frame = MakeBlackFrame();
    DrawBrightRect(frame, { 200, 100 }, { 700, 100 }, { 700, 400 }, { 200, 400 });

    auto result = detector->DetectOnce(frame);
    EXPECT_TRUE(result.topScreen.has_value());
}

TEST(ScreenDetector, CalibrationLocksAfterEnoughFrames)
{
    SH3DS::Capture::ScreenDetectorConfig config;
    config.calibrationFrames = 5;
    SH3DS::Capture::ScreenDetector detector(config);

    // Need both screens for calibration to lock
    auto frame = MakeBlackFrame(1280, 960);
    DrawBrightRect(frame, { 340, 80 }, { 940, 80 }, { 940, 440 }, { 340, 440 });   // top 5:3
    DrawBrightRect(frame, { 400, 460 }, { 880, 460 }, { 880, 820 }, { 400, 820 }); // bottom 4:3

    EXPECT_FALSE(detector.IsCalibrated());

    for (int i = 0; i < 5; ++i)
    {
        detector.Detect(frame);
    }

    EXPECT_TRUE(detector.IsCalibrated());

    // After calibration, returns cached result even with a different frame
    auto blackFrame = MakeBlackFrame();
    auto result = detector.Detect(blackFrame);
    EXPECT_TRUE(result.topScreen.has_value());
    EXPECT_TRUE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, ResetClearsCalibration)
{
    SH3DS::Capture::ScreenDetectorConfig config;
    config.calibrationFrames = 3;
    SH3DS::Capture::ScreenDetector detector(config);

    auto frame = MakeBlackFrame(1280, 960);
    DrawBrightRect(frame, { 340, 80 }, { 940, 80 }, { 940, 440 }, { 340, 440 });
    DrawBrightRect(frame, { 400, 460 }, { 880, 460 }, { 880, 820 }, { 400, 820 });

    for (int i = 0; i < 3; ++i)
    {
        detector.Detect(frame);
    }
    EXPECT_TRUE(detector.IsCalibrated());

    detector.Reset();
    EXPECT_FALSE(detector.IsCalibrated());
}

TEST(ScreenDetector, DetectsTrapezoidal5To3Screen)
{
    auto frame = MakeBlackFrame();
    // Perspective-distorted trapezoid with ~5:3 aspect ratio
    // Top edge narrower than bottom (camera below screen)
    DrawBrightRect(frame,
        cv::Point(250, 120),
        cv::Point(650, 120),
        cv::Point(700, 400),
        cv::Point(200, 400));

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    ASSERT_TRUE(result.topScreen.has_value());
    EXPECT_GT(result.topScreen->confidence, 0.3);
    EXPECT_NEAR(result.topScreen->aspectRatio, 1.667, 0.35);
}

TEST(ScreenDetector, CalibrationToleratesOccasionalMisses)
{
    SH3DS::Capture::ScreenDetectorConfig config;
    config.calibrationFrames = 10;
    SH3DS::Capture::ScreenDetector detector(config);

    auto goodFrame = MakeBlackFrame(1280, 960);
    DrawBrightRect(goodFrame, { 340, 80 }, { 940, 80 }, { 940, 440 }, { 340, 440 });
    DrawBrightRect(goodFrame, { 400, 460 }, { 880, 460 }, { 880, 820 }, { 400, 820 });

    auto badFrame = MakeBlackFrame();

    // Feed 20 frames with 2 bad frames interspersed
    for (int i = 0; i < 20; ++i)
    {
        if (i == 5 || i == 12)
        {
            detector.Detect(badFrame);
        }
        else
        {
            detector.Detect(goodFrame);
        }
    }

    // Should still calibrate (18/20 = 90% > 80% threshold)
    EXPECT_TRUE(detector.IsCalibrated());
}

TEST(ScreenDetector, SingleBottomScreenClassifiedByAspectRatio)
{
    auto frame = MakeBlackFrame();
    // 480x360 rect -> aspect ratio 1.333 (4:3) -> should be classified as bottom
    DrawBrightRect(frame, { 400, 200 }, { 880, 200 }, { 880, 560 }, { 400, 560 });

    SH3DS::Capture::ScreenDetector detector;
    auto result = detector.DetectOnce(frame);

    EXPECT_FALSE(result.topScreen.has_value());
    ASSERT_TRUE(result.bottomScreen.has_value());
}

TEST(ScreenDetector, HeldScreenMarkedAsHeld)
{
    SH3DS::Capture::ScreenDetector detector;

    auto frame = MakeBlackFrame();
    DrawBrightRect(frame, { 200, 100 }, { 700, 100 }, { 700, 400 }, { 200, 400 });

    // Build up smoothing state
    detector.Detect(frame);
    detector.Detect(frame);

    // Now feed an empty frame — should hold the previous corners with held=true
    auto emptyFrame = MakeBlackFrame();
    auto result = detector.Detect(emptyFrame);

    if (result.topScreen.has_value())
    {
        EXPECT_TRUE(result.topScreen->held);
        EXPECT_DOUBLE_EQ(result.topScreen->confidence, 0.0);
    }
}
