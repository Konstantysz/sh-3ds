#pragma once

#include "Core/Config.h"
#include "FrameSource.h"

#include <opencv2/videoio.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace SH3DS::Capture
{
    /**
     * @brief Reads frames from a live MJPEG stream via cv::VideoCapture (ffmpeg backend).
     *
     * Implements FrameSource only — does not implement FrameSeeker (live streams
     * are not seekable). On read failure, attempts reconnection up to
     * maxReconnectAttempts times before entering permanently-failed state.
     */
    class MjpegFrameSource : public FrameSource
    {
    public:
        /**
         * @brief Constructs a new MjpegFrameSource.
         * @param config Camera configuration (uri, reconnect settings, timeout).
         */
        explicit MjpegFrameSource(Core::CameraConfig config);

        bool Open() override;
        void Close() override;
        std::optional<Core::Frame> Grab() override;
        bool IsOpen() const override;
        std::string Describe() const override;

        /**
         * @brief Factory — creates a MjpegFrameSource wrapped in a FrameSource unique_ptr.
         * @param config Camera configuration.
         * @return Non-null unique_ptr to a FrameSource.
         */
        static std::unique_ptr<FrameSource> CreateMjpegFrameSource(const Core::CameraConfig &config);

    private:
        /**
         * @brief Attempts to reconnect up to maxReconnectAttempts times.
         * @return True if reconnection succeeded.
         */
        bool TryReconnect();

        std::string uri;                  ///< Stream URI (HTTP URL or local file path)
        int reconnectDelayMs;             ///< Delay between reconnect attempts in ms
        int maxReconnectAttempts;         ///< Maximum number of reconnect attempts
        int grabTimeoutMs;                ///< Open/read timeout in ms (passed to VideoCapture)
        cv::VideoCapture capture;         ///< OpenCV capture object
        mutable std::mutex mutex;         ///< Guards capture and frame counter
        size_t frameCounter = 0;          ///< Monotonically increasing sequence number
        int currentReconnectAttempts = 0; ///< Reconnect attempts used in last failure run
        bool isOpen = false;              ///< Whether capture is currently open
        bool permanentlyFailed = false;   ///< True after exhausting all reconnect attempts
    };

} // namespace SH3DS::Capture
