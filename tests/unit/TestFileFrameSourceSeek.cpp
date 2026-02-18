#include "Capture/FileFrameSource.h"
#include "Capture/FrameSeeker.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

    std::filesystem::path CreateTempDir(const std::string &name)
    {
        auto dir = std::filesystem::temp_directory_path() / ("sh3ds_test_" + name);
        std::filesystem::create_directories(dir);
        return dir;
    }

    void CreateDummyImage(const std::filesystem::path &path, int width, int height)
    {
        // Minimal valid BMP: 54-byte header + pixel data
        int rowSize = ((width * 3 + 3) / 4) * 4;
        int imageSize = rowSize * height;
        int fileSize = 54 + imageSize;

        std::vector<uint8_t> bmp(static_cast<size_t>(fileSize), 0);

        // BMP header
        bmp[0] = 'B';
        bmp[1] = 'M';
        *reinterpret_cast<int32_t *>(&bmp[2]) = fileSize;
        *reinterpret_cast<int32_t *>(&bmp[10]) = 54;

        // DIB header
        *reinterpret_cast<int32_t *>(&bmp[14]) = 40;
        *reinterpret_cast<int32_t *>(&bmp[18]) = width;
        *reinterpret_cast<int32_t *>(&bmp[22]) = height;
        *reinterpret_cast<int16_t *>(&bmp[26]) = 1;
        *reinterpret_cast<int16_t *>(&bmp[28]) = 24;
        *reinterpret_cast<int32_t *>(&bmp[34]) = imageSize;

        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bmp.data()), fileSize);
    }

    class FileFrameSourceSeekTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            testDir = CreateTempDir("seek");
            std::filesystem::remove_all(testDir);
            std::filesystem::create_directories(testDir);

            for (int i = 0; i < 5; ++i)
            {
                auto filename = "frame_" + std::to_string(i) + ".bmp";
                CreateDummyImage(testDir / filename, 16, 16);
            }
        }

        void TearDown() override
        {
            std::filesystem::remove_all(testDir);
        }

        std::filesystem::path testDir;
    };

} // namespace

TEST_F(FileFrameSourceSeekTest, ImplementsFrameSeeker)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    SH3DS::Capture::FrameSeeker *seeker = &source;
    EXPECT_NE(seeker, nullptr);
}

TEST_F(FileFrameSourceSeekTest, GetFrameCountReturnsCorrectCount)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    source.Open();

    EXPECT_EQ(source.GetFrameCount(), 5u);
}

TEST_F(FileFrameSourceSeekTest, SeekToValidIndex)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    source.Open();

    EXPECT_TRUE(source.Seek(3));
    auto frame = source.Grab();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->metadata.sequenceNumber, 3u);
}

TEST_F(FileFrameSourceSeekTest, SeekToFirstFrame)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    source.Open();

    // Advance past first frame
    source.Grab();
    source.Grab();

    // Seek back to 0
    EXPECT_TRUE(source.Seek(0));
    auto frame = source.Grab();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->metadata.sequenceNumber, 0u);
}

TEST_F(FileFrameSourceSeekTest, SeekOutOfBoundsReturnsFalse)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    source.Open();

    EXPECT_FALSE(source.Seek(5));
    EXPECT_FALSE(source.Seek(100));
}

TEST_F(FileFrameSourceSeekTest, SeekOnSingleFrameSource)
{
    auto singleDir = CreateTempDir("seek_single");
    std::filesystem::remove_all(singleDir);
    std::filesystem::create_directories(singleDir);
    CreateDummyImage(singleDir / "frame_0.bmp", 16, 16);

    SH3DS::Capture::FileFrameSource source(singleDir);
    source.Open();

    EXPECT_EQ(source.GetFrameCount(), 1u);
    EXPECT_TRUE(source.Seek(0));
    EXPECT_FALSE(source.Seek(1));

    std::filesystem::remove_all(singleDir);
}

TEST_F(FileFrameSourceSeekTest, SeekOnEmptySource)
{
    auto emptyDir = CreateTempDir("seek_empty");
    std::filesystem::remove_all(emptyDir);
    std::filesystem::create_directories(emptyDir);

    SH3DS::Capture::FileFrameSource source(emptyDir);
    source.Open();

    EXPECT_EQ(source.GetFrameCount(), 0u);
    EXPECT_FALSE(source.Seek(0));

    std::filesystem::remove_all(emptyDir);
}

TEST_F(FileFrameSourceSeekTest, SeekViaFrameSeekerInterface)
{
    SH3DS::Capture::FileFrameSource source(testDir);
    source.Open();

    SH3DS::Capture::FrameSeeker *seeker = &source;
    EXPECT_EQ(seeker->GetFrameCount(), 5u);
    EXPECT_TRUE(seeker->Seek(2));
}
