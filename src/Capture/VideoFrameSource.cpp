#include "VideoFrameSource.h"

#include "Kappa/Logger.h"

namespace SH3DS::Capture
{
    VideoFrameSource::VideoFrameSource(const std::filesystem::path &videoPath, double playbackFps)
        : videoPath(videoPath),
          playbackFps(playbackFps)
    {
    }

    bool VideoFrameSource::Open()
    {
        if (!std::filesystem::exists(videoPath))
        {
            LOG_ERROR("VideoFrameSource: File does not exist: {}", videoPath.string());
            return false;
        }

        if (!capture.open(videoPath.string()))
        {
            LOG_ERROR("VideoFrameSource: Failed to open video: {}", videoPath.string());
            return false;
        }

        totalFrames = static_cast<size_t>(capture.get(cv::CAP_PROP_FRAME_COUNT));
        nativeFps = capture.get(cv::CAP_PROP_FPS);

        if (playbackFps <= 0.0)
        {
            playbackFps = nativeFps;
        }

        currentIndex = 0;
        isOpen = totalFrames > 0;

        LOG_INFO(
            "VideoFrameSource: Opened {} ({} frames, {:.1f} native FPS)", videoPath.string(), totalFrames, nativeFps);

        return isOpen;
    }

    void VideoFrameSource::Close()
    {
        capture.release();
        currentIndex = 0;
        totalFrames = 0;
        isOpen = false;
    }

    std::optional<Core::Frame> VideoFrameSource::Grab()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!isOpen || currentIndex >= totalFrames)
        {
            return std::nullopt;
        }

        cv::Mat image;
        if (!capture.read(image) || image.empty())
        {
            LOG_WARN("VideoFrameSource: Failed to read frame {}", currentIndex);
            ++currentIndex;
            return std::nullopt;
        }

        if (image.channels() != 3 && image.channels() != 4)
        {
            LOG_WARN(
                "VideoFrameSource: Unexpected frame format at index {}: channels={}", currentIndex, image.channels());
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

    bool VideoFrameSource::IsOpen() const
    {
        return isOpen && currentIndex < totalFrames;
    }

    std::string VideoFrameSource::Describe() const
    {
        return "VideoFrameSource(" + videoPath.string() + ", " + std::to_string(totalFrames) + " frames)";
    }

    bool VideoFrameSource::Seek(size_t frameIndex)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (frameIndex >= totalFrames)
        {
            return false;
        }

        if (!capture.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frameIndex)))
        {
            LOG_WARN("VideoFrameSource: Failed to seek to frame {}", frameIndex);
            return false;
        }

        currentIndex = frameIndex;
        return true;
    }

    size_t VideoFrameSource::GetFrameCount() const
    {
        return totalFrames;
    }

    std::unique_ptr<FrameSource> VideoFrameSource::CreateVideoFrameSource(const std::filesystem::path &videoPath,
        double playbackFps)
    {
        return std::make_unique<VideoFrameSource>(videoPath, playbackFps);
    }

} // namespace SH3DS::Capture
