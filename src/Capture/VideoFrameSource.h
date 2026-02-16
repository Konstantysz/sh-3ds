#pragma once

#include "FrameSeeker.h"
#include "FrameSource.h"

#include <opencv2/videoio.hpp>

#include <filesystem>
#include <mutex>

namespace SH3DS::Capture
{
    /**
     * @brief Reads frames from a video file (e.g., .mp4) with seek support.
     */
    class VideoFrameSource
        : public FrameSource
        , public FrameSeeker
    {
    public:
        /**
         * @brief Constructs a new VideoFrameSource.
         * @param videoPath Path to the video file.
         * @param playbackFps Playback speed override (0 = use video's native FPS).
         */
        explicit VideoFrameSource(const std::filesystem::path &videoPath, double playbackFps = 0.0);

        bool Open() override;
        void Close() override;
        std::optional<Core::Frame> Grab() override;
        bool IsOpen() const override;
        std::string Describe() const override;

        bool Seek(size_t frameIndex) override;
        size_t GetFrameCount() const override;

        /**
         * @brief Creates a video frame source from a video file.
         * @param videoPath Path to the video file.
         * @param playbackFps Playback speed override (0 = use video's native FPS).
         * @return A unique pointer to the frame source.
         */
        static std::unique_ptr<FrameSource> CreateVideoFrameSource(const std::filesystem::path &videoPath,
            double playbackFps);

    private:
        std::filesystem::path videoPath; ///< Path to the video file
        double playbackFps;              ///< Playback FPS override
        cv::VideoCapture capture;        ///< OpenCV video capture
        size_t currentIndex = 0;         ///< Current frame index
        size_t totalFrames = 0;          ///< Total number of frames
        double nativeFps = 0.0;          ///< Video's native FPS
        bool isOpen = false;             ///< Whether the video is open
        mutable std::mutex mutex;        ///< Guards cv::VideoCapture and currentIndex
    };
} // namespace SH3DS::Capture
