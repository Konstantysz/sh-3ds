#include "Capture/FrameSeeker.h"
#include "Capture/MjpegFrameSource.h"
#include "Core/Config.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace
{

    class MjpegFrameSourceTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            videoPath = std::filesystem::temp_directory_path() / "sh3ds_test_mjpeg.avi";

            cv::VideoWriter writer(
                videoPath.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 10.0, cv::Size(64, 48));

            if (!writer.isOpened())
            {
                GTEST_SKIP() << "Cannot create test MJPEG video (no codec support)";
            }

            for (int i = 0; i < 5; ++i)
            {
                cv::Mat frame(48, 64, CV_8UC3, cv::Scalar(i * 40, 100, 200));
                writer.write(frame);
            }
            writer.release();

            if (!std::filesystem::exists(videoPath) || std::filesystem::file_size(videoPath) == 0)
            {
                GTEST_SKIP() << "Test MJPEG video was not created successfully";
            }
        }

        void TearDown() override
        {
            std::filesystem::remove(videoPath);
        }

        SH3DS::Core::CameraConfig MakeConfig(const std::string &uri) const
        {
            SH3DS::Core::CameraConfig cfg;
            cfg.type = "mjpeg";
            cfg.uri = uri;
            cfg.reconnectDelayMs = 0;
            cfg.maxReconnectAttempts = 0;
            cfg.grabTimeoutMs = 2000;
            return cfg;
        }

        std::filesystem::path videoPath;
    };

} // namespace

TEST_F(MjpegFrameSourceTest, OpenFailsOnUnreachableUri)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig("http://127.0.0.1:1/video"));
    EXPECT_FALSE(source.Open());
    EXPECT_FALSE(source.IsOpen());
}

TEST_F(MjpegFrameSourceTest, OpenSucceedsOnLocalFileUri)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(videoPath.string()));
    EXPECT_TRUE(source.Open());
    EXPECT_TRUE(source.IsOpen());
}

TEST_F(MjpegFrameSourceTest, GrabReturnsFrameWithCorrectMetadata)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(videoPath.string()));
    source.Open();

    auto frame = source.Grab();
    ASSERT_TRUE(frame.has_value());
    EXPECT_FALSE(frame->image.empty());
    EXPECT_EQ(frame->metadata.sequenceNumber, 0u);
    EXPECT_GT(frame->metadata.captureTime.time_since_epoch().count(), 0);

    auto frame2 = source.Grab();
    ASSERT_TRUE(frame2.has_value());
    EXPECT_EQ(frame2->metadata.sequenceNumber, 1u);
}

TEST_F(MjpegFrameSourceTest, GrabReturnsNulloptWhenExhausted)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(videoPath.string()));
    source.Open();

    // Drain all 5 frames
    for (int i = 0; i < 5; ++i)
    {
        source.Grab();
    }

    auto frame = source.Grab();
    EXPECT_FALSE(frame.has_value());
}

TEST_F(MjpegFrameSourceTest, CloseReleasesCapture)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(videoPath.string()));
    source.Open();
    EXPECT_TRUE(source.IsOpen());

    source.Close();
    EXPECT_FALSE(source.IsOpen());

    auto frame = source.Grab();
    EXPECT_FALSE(frame.has_value());
}

TEST_F(MjpegFrameSourceTest, DescribeContainsUri)
{
    const std::string uri = videoPath.string();
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(uri));

    std::string desc = source.Describe();
    EXPECT_NE(desc.find("MjpegFrameSource"), std::string::npos);
    EXPECT_NE(desc.find(uri), std::string::npos);
}

TEST_F(MjpegFrameSourceTest, FactoryReturnsNonNullPtr)
{
    auto source = SH3DS::Capture::MjpegFrameSource::CreateMjpegFrameSource(MakeConfig(videoPath.string()));
    ASSERT_NE(source, nullptr);
}

TEST_F(MjpegFrameSourceTest, DoesNotImplementFrameSeeker)
{
    SH3DS::Capture::MjpegFrameSource source(MakeConfig(videoPath.string()));
    SH3DS::Capture::FrameSeeker *seeker = dynamic_cast<SH3DS::Capture::FrameSeeker *>(&source);
    EXPECT_EQ(seeker, nullptr);
}
