#include "FileFrameSource.h"

#include "Kappa/Logger.h"

#include <opencv2/imgcodecs.hpp>

namespace SH3DS::Capture
{
    FileFrameSource::FileFrameSource(const std::filesystem::path &directory, double playbackFps)
        : directory(directory),
          playbackFps(playbackFps)
    {
    }

    bool FileFrameSource::Open()
    {
        if (!std::filesystem::is_directory(directory))
        {
            LOG_ERROR("FileFrameSource: Directory does not exist: {}", directory.string());
            return false;
        }

        framePaths.clear();
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
            {
                framePaths.push_back(entry.path().string());
            }
        }

        std::sort(framePaths.begin(), framePaths.end());
        currentIndex = 0;
        open = !framePaths.empty();
        return open;
    }

    void FileFrameSource::Close()
    {
        framePaths.clear();
        currentIndex = 0;
        open = false;
    }

    std::optional<Core::Frame> FileFrameSource::Grab()
    {
        if (!open || currentIndex >= framePaths.size())
        {
            return std::nullopt;
        }

        cv::Mat image = cv::imread(framePaths[currentIndex].string(), cv::IMREAD_COLOR);
        if (image.empty())
        {
            LOG_WARN("FileFrameSource: Failed to read image: {}", framePaths[currentIndex].string());
            ++currentIndex;
            return std::nullopt;
        }

        Core::Frame frame;
        frame.image = image;
        frame.metadata.sequenceNumber = currentIndex;
        frame.metadata.captureTime = std::chrono::steady_clock::now();
        frame.metadata.sourceWidth = image.cols;
        frame.metadata.sourceHeight = image.rows;
        frame.metadata.fpsEstimate = playbackFps;

        ++currentIndex;
        return frame;
    }

    bool FileFrameSource::IsOpen() const
    {
        return open && currentIndex < framePaths.size();
    }

    std::string FileFrameSource::Describe() const
    {
        return "FileFrameSource(" + directory.string() + ", " + std::to_string(framePaths.size()) + " frames)";
    }

    bool FileFrameSource::Seek(size_t frameIndex)
    {
        if (frameIndex >= framePaths.size())
        {
            return false;
        }
        currentIndex = frameIndex;
        return true;
    }

    size_t FileFrameSource::GetFrameCount() const
    {
        return framePaths.size();
    }

    std::unique_ptr<FrameSource> FileFrameSource::CreateFileFrameSource(const std::filesystem::path &directory,
        double playbackFps)
    {
        return std::make_unique<FileFrameSource>(directory, playbackFps);
    }

} // namespace SH3DS::Capture
