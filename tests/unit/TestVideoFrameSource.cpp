#include "Capture/FrameSeeker.h"
#include "Capture/VideoFrameSource.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace
{

    class VideoFrameSourceTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            videoPath = std::filesystem::temp_directory_path() / "sh3ds_test_video.avi";

            // Create a tiny test video with 10 frames using MJPG codec
            cv::VideoWriter writer(
                videoPath.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 10.0, cv::Size(64, 48));

            if (!writer.isOpened())
            {
                GTEST_SKIP() << "Cannot create test video (no codec support)";
            }

            for (int i = 0; i < 10; ++i)
            {
                cv::Mat frame(48, 64, CV_8UC3, cv::Scalar(i * 25, 100, 200));
                writer.write(frame);
            }
            writer.release();

            // Verify the file exists and is non-empty
            if (!std::filesystem::exists(videoPath) || std::filesystem::file_size(videoPath) == 0)
            {
                GTEST_SKIP() << "Test video was not created successfully";
            }
        }

        void TearDown() override
        {
            std::filesystem::remove(videoPath);
        }

        std::filesystem::path videoPath;
    };

} // namespace

TEST_F(VideoFrameSourceTest, OpenSucceeds)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    EXPECT_TRUE(source.Open());
    EXPECT_TRUE(source.IsOpen());
}

TEST_F(VideoFrameSourceTest, OpenNonexistentFails)
{
    SH3DS::Capture::VideoFrameSource source("nonexistent_video.mp4");
    EXPECT_FALSE(source.Open());
    EXPECT_FALSE(source.IsOpen());
}

TEST_F(VideoFrameSourceTest, GetFrameCountReturnsCorrectCount)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();

    EXPECT_EQ(source.GetFrameCount(), 10u);
}

TEST_F(VideoFrameSourceTest, GrabReturnsFrameWithCorrectDimensions)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();

    auto frame = source.Grab();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->image.cols, 64);
    EXPECT_EQ(frame->image.rows, 48);
    EXPECT_EQ(frame->metadata.sequenceNumber, 0u);
}

TEST_F(VideoFrameSourceTest, SeekToValidIndex)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();

    EXPECT_TRUE(source.Seek(5));
    auto frame = source.Grab();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->metadata.sequenceNumber, 5u);
}

TEST_F(VideoFrameSourceTest, SeekOutOfBounds)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();

    EXPECT_FALSE(source.Seek(10));
    EXPECT_FALSE(source.Seek(100));
}

TEST_F(VideoFrameSourceTest, ImplementsFrameSeeker)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    SH3DS::Capture::FrameSeeker *seeker = &source;

    source.Open();
    EXPECT_EQ(seeker->GetFrameCount(), 10u);
    EXPECT_TRUE(seeker->Seek(3));
}

TEST_F(VideoFrameSourceTest, CloseResetsState)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();
    EXPECT_TRUE(source.IsOpen());

    source.Close();
    EXPECT_FALSE(source.IsOpen());
}

TEST_F(VideoFrameSourceTest, DescribeContainsPath)
{
    SH3DS::Capture::VideoFrameSource source(videoPath);
    source.Open();

    std::string desc = source.Describe();
    EXPECT_NE(desc.find("VideoFrameSource"), std::string::npos);
    EXPECT_NE(desc.find("10 frames"), std::string::npos);
}

TEST_F(VideoFrameSourceTest, FactoryReturnsValidSource)
{
    auto source = SH3DS::Capture::VideoFrameSource::CreateVideoFrameSource(videoPath, 0.0);
    ASSERT_NE(source, nullptr);
    EXPECT_TRUE(source->Open());
}
