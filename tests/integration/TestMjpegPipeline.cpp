#include "Capture/FramePreprocessor.h"
#include "Capture/FrameSeeker.h"
#include "Capture/MjpegFrameSource.h"
#include "Core/Config.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>

#include <filesystem>

/// Integration test: MjpegFrameSource → FramePreprocessor pipeline using a local MJPEG file.
/// No network camera or real 3DS stream required — all frames are synthetic.
namespace
{

    class MjpegPipelineTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            videoPath = std::filesystem::temp_directory_path() / "sh3ds_integration_mjpeg.avi";

            cv::VideoWriter writer(
                videoPath.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 10.0, cv::Size(400, 240));

            if (!writer.isOpened())
            {
                GTEST_SKIP() << "Cannot create MJPEG test video (no codec support)";
            }

            for (int i = 0; i < 5; ++i)
            {
                // Alternate dark / bright frames to exercise a simple FSM-style detection
                cv::Scalar color = (i % 2 == 0) ? cv::Scalar(20, 20, 20) : cv::Scalar(220, 220, 220);
                cv::Mat frame(240, 400, CV_8UC3, color);
                writer.write(frame);
            }
            writer.release();

            if (!std::filesystem::exists(videoPath) || std::filesystem::file_size(videoPath) == 0)
            {
                GTEST_SKIP() << "Integration MJPEG video was not created successfully";
            }

            // Identity-warp calibration: source frame is already 400x240
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
        }

        void TearDown() override
        {
            std::filesystem::remove(videoPath);
        }

        SH3DS::Core::CameraConfig MakeConfig() const
        {
            SH3DS::Core::CameraConfig cfg;
            cfg.type = "mjpeg";
            cfg.uri = videoPath.string();
            cfg.reconnectDelayMs = 0;
            cfg.maxReconnectAttempts = 0;
            cfg.grabTimeoutMs = 2000;
            return cfg;
        }

        std::filesystem::path videoPath;
        SH3DS::Core::ScreenCalibrationConfig calibration;
        std::vector<SH3DS::Core::RoiDefinition> rois;
    };

} // namespace

TEST_F(MjpegPipelineTest, MjpegSourceProducesFrames)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig());
    ASSERT_TRUE(source.Open());

    int grabbed = 0;
    while (true)
    {
        auto frame = source.Grab();
        if (!frame)
        {
            break;
        }
        EXPECT_FALSE(frame->image.empty());
        EXPECT_EQ(frame->image.cols, 400);
        EXPECT_EQ(frame->image.rows, 240);
        EXPECT_EQ(frame->metadata.sequenceNumber, static_cast<size_t>(grabbed));
        ++grabbed;
    }

    EXPECT_EQ(grabbed, 5);
}

TEST_F(MjpegPipelineTest, FramesPassThroughPreprocessor)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig());
    ASSERT_TRUE(source.Open());

    auto preprocessor = std::make_unique<SH3DS::Capture::FramePreprocessor>(calibration, rois);

    int processed = 0;
    while (true)
    {
        auto frame = source.Grab();
        if (!frame)
        {
            break;
        }

        auto roiSet = preprocessor->Process(frame->image);
        ASSERT_TRUE(roiSet.has_value());
        EXPECT_FALSE(roiSet->empty());
        EXPECT_NE(roiSet->find("full_screen"), roiSet->end());
        EXPECT_NE(roiSet->find("pokemon_sprite"), roiSet->end());
        ++processed;
    }

    EXPECT_EQ(processed, 5);
}

TEST_F(MjpegPipelineTest, SourceDoesNotImplementFrameSeeker)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig());
    SH3DS::Capture::FrameSeeker *seeker = dynamic_cast<SH3DS::Capture::FrameSeeker *>(&source);
    EXPECT_EQ(seeker, nullptr);
}

TEST_F(MjpegPipelineTest, PipelineHandlesStreamEnd)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig());
    ASSERT_TRUE(source.Open());

    auto preprocessor = std::make_unique<SH3DS::Capture::FramePreprocessor>(calibration, rois);

    // Drain all frames
    while (true)
    {
        auto frame = source.Grab();
        if (!frame)
        {
            break;
        }
        // Pipeline must not crash on any frame
        preprocessor->Process(frame->image);
    }

    // Further grabs after stream end must return nullopt without crashing
    EXPECT_FALSE(source.Grab().has_value());
    EXPECT_FALSE(source.Grab().has_value());
}
