#include "Core/Config.h"
#include "Core/Types.h"
#include "Vision/HistogramUtils.h"
#include "Vision/DominantColorDetector.h"
#include "Vision/HistogramDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /// Create a synthetic BGR image filled with a specific HSV color.
    cv::Mat CreateColoredImage(int h, int s, int v, int width = 100, int height = 100)
    {
        cv::Mat hsv(height, width, CV_8UC3, cv::Scalar(h, s, v));
        cv::Mat bgr;
        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
        return bgr;
    }

    SH3DS::Core::DetectionMethodConfig CreateFroakieConfig()
    {
        SH3DS::Core::DetectionMethodConfig config;
        config.method = "dominant_color";
        config.weight = 1.0;
        // Normal Froakie: dark blue
        config.normalHsvLower = cv::Scalar(100, 100, 60);
        config.normalHsvUpper = cv::Scalar(130, 255, 200);
        // Shiny Froakie: pastel/light blue
        config.shinyHsvLower = cv::Scalar(95, 25, 170);
        config.shinyHsvUpper = cv::Scalar(135, 110, 255);
        config.shinyRatioThreshold = 0.12;
        config.normalRatioThreshold = 0.12;
        return config;
    }

    SH3DS::Core::DetectionMethodConfig CreateHistogramFroakieConfig(
        const std::string &normalRefPath,
        const std::string &shinyRefPath)
    {
        SH3DS::Core::DetectionMethodConfig config;
        config.method = "histogram_compare";
        config.weight = 1.0;
        config.referenceNormal = normalRefPath;
        config.referenceShiny = shinyRefPath;
        config.compareMethod = "correlation";
        config.differentialThreshold = 0.15;
        return config;
    }
} // namespace

// ============================================================================
// DominantColorDetector tests
// ============================================================================

TEST(DominantColorDetector, DetectsNormalFroakie)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    // Normal Froakie color: dark blue (H=115, S=180, V=130)
    auto normalImage = CreateColoredImage(115, 180, 130);

    auto result = detector->Detect(normalImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::NotShiny);
    EXPECT_GT(result.confidence, 0.0);
    EXPECT_EQ(result.method, "dominant_color");
}

TEST(DominantColorDetector, DetectsShinyFroakie)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    // Shiny Froakie color: pastel blue (H=115, S=60, V=220)
    auto shinyImage = CreateColoredImage(115, 60, 220);

    auto result = detector->Detect(shinyImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Shiny);
    EXPECT_GT(result.confidence, 0.0);
    EXPECT_EQ(result.method, "dominant_color");
}

TEST(DominantColorDetector, UncertainOnUnrelatedColor)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    // Red image — neither normal nor shiny Froakie
    auto redImage = CreateColoredImage(0, 255, 200);

    auto result = detector->Detect(redImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Uncertain);
}

TEST(DominantColorDetector, EmptyImageReturnsUncertain)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    cv::Mat empty;
    auto result = detector->Detect(empty);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Uncertain);
}

TEST(DominantColorDetector, ProfileIdMatches)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "xy_froakie");
    EXPECT_EQ(detector->ProfileId(), "xy_froakie");
}

TEST(DominantColorDetector, DetectSequenceUsesMiddleFrame)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    // Sequence of 3 normal images
    std::vector<cv::Mat> frames;
    for (int i = 0; i < 3; ++i)
    {
        frames.push_back(CreateColoredImage(115, 180, 130));
    }

    auto result = detector->DetectSequence(frames);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::NotShiny);
}

TEST(DominantColorDetector, NormalAndShinyAreDistinguishable)
{
    auto config = CreateFroakieConfig();
    auto detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(config, "test_froakie");

    auto normalImage = CreateColoredImage(115, 180, 130);
    auto shinyImage = CreateColoredImage(115, 60, 220);

    auto normalResult = detector->Detect(normalImage);
    auto shinyResult = detector->Detect(shinyImage);

    EXPECT_NE(normalResult.verdict, shinyResult.verdict);
    EXPECT_EQ(normalResult.verdict, SH3DS::Core::ShinyVerdict::NotShiny);
    EXPECT_EQ(shinyResult.verdict, SH3DS::Core::ShinyVerdict::Shiny);
}

// ============================================================================
// HistogramDetector tests
// ============================================================================

class HistogramDetectorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create synthetic normal (dark blue) and shiny (pastel blue) images
        auto normalImage = CreateColoredImage(115, 180, 130);
        auto shinyImage = CreateColoredImage(115, 60, 220);

        // Compute reference histograms
        auto normalHist = SH3DS::Vision::ComputeHSHistogram(normalImage, 30, 32);
        auto shinyHist = SH3DS::Vision::ComputeHSHistogram(shinyImage, 30, 32);

        // Write to temp files
        normalRefPath = (std::filesystem::temp_directory_path() / "test_normal_ref.yml").string();
        shinyRefPath = (std::filesystem::temp_directory_path() / "test_shiny_ref.yml").string();

        SH3DS::Vision::SaveHistogram(normalHist, normalRefPath);
        SH3DS::Vision::SaveHistogram(shinyHist, shinyRefPath);
    }

    void TearDown() override
    {
        std::filesystem::remove(normalRefPath);
        std::filesystem::remove(shinyRefPath);
    }

    std::string normalRefPath;
    std::string shinyRefPath;
};

TEST_F(HistogramDetectorTest, MissingReferencesReturnUncertain)
{
    SH3DS::Core::DetectionMethodConfig config;
    config.method = "histogram_compare";
    config.compareMethod = "correlation";
    config.differentialThreshold = 0.15;
    // referenceNormal and referenceShiny left empty

    auto detector = SH3DS::Vision::HistogramDetector::CreateHistogramDetector(config, "test_froakie");
    auto normalImage = CreateColoredImage(115, 180, 130);

    auto result = detector->Detect(normalImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Uncertain);
}

TEST_F(HistogramDetectorTest, DetectsNormalViaHistogram)
{
    auto config = CreateHistogramFroakieConfig(normalRefPath, shinyRefPath);
    auto detector = SH3DS::Vision::HistogramDetector::CreateHistogramDetector(config, "test_froakie");

    // Image matching the "normal" reference
    auto normalImage = CreateColoredImage(115, 180, 130);

    auto result = detector->Detect(normalImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::NotShiny);
    EXPECT_GT(result.confidence, 0.0);
    EXPECT_EQ(result.method, "histogram_compare");
}

TEST_F(HistogramDetectorTest, DetectsShinyViaHistogram)
{
    auto config = CreateHistogramFroakieConfig(normalRefPath, shinyRefPath);
    auto detector = SH3DS::Vision::HistogramDetector::CreateHistogramDetector(config, "test_froakie");

    // Image matching the "shiny" reference
    auto shinyImage = CreateColoredImage(115, 60, 220);

    auto result = detector->Detect(shinyImage);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Shiny);
    EXPECT_GT(result.confidence, 0.0);
    EXPECT_EQ(result.method, "histogram_compare");
}

TEST_F(HistogramDetectorTest, ProfileIdMatches)
{
    auto config = CreateHistogramFroakieConfig(normalRefPath, shinyRefPath);
    auto detector = SH3DS::Vision::HistogramDetector::CreateHistogramDetector(config, "xy_froakie_hist");
    EXPECT_EQ(detector->ProfileId(), "xy_froakie_hist");
}

TEST_F(HistogramDetectorTest, EmptyImageReturnsUncertain)
{
    auto config = CreateHistogramFroakieConfig(normalRefPath, shinyRefPath);
    auto detector = SH3DS::Vision::HistogramDetector::CreateHistogramDetector(config, "test_froakie");

    cv::Mat empty;
    auto result = detector->Detect(empty);
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Uncertain);
}
