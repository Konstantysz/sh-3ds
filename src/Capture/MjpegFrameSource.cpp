#include "MjpegFrameSource.h"

#include "Kappa/Logger.h"

#include <chrono>
#include <thread>

namespace SH3DS::Capture
{
    MjpegFrameSource::MjpegFrameSource(Core::CameraConfig config)
        : uri(std::move(config.uri)),
          reconnectDelayMs(config.reconnectDelayMs),
          maxReconnectAttempts(config.maxReconnectAttempts),
          grabTimeoutMs(config.grabTimeoutMs)
    {
    }

    bool MjpegFrameSource::Open()
    {
        std::lock_guard<std::mutex> lock(mutex);

        permanentlyFailed = false;
        currentReconnectAttempts = 0;
        frameCounter = 0;

        capture.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, static_cast<double>(grabTimeoutMs));
        capture.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, static_cast<double>(grabTimeoutMs));

        if (!capture.open(uri))
        {
            LOG_ERROR("MjpegFrameSource: Failed to open URI: {}", uri);
            isOpen = false;
            return false;
        }

        isOpen = true;
        LOG_INFO("MjpegFrameSource: Opened {}", uri);
        return true;
    }

    void MjpegFrameSource::Close()
    {
        std::lock_guard<std::mutex> lock(mutex);

        capture.release();
        isOpen = false;
        frameCounter = 0;
        currentReconnectAttempts = 0;
        permanentlyFailed = false;
    }

    std::optional<Core::Frame> MjpegFrameSource::Grab()
    {
        std::unique_lock<std::mutex> lock(mutex);

        if (!isOpen || permanentlyFailed)
        {
            return std::nullopt;
        }

        cv::Mat image;
        if (!capture.read(image) || image.empty())
        {
            LOG_WARN("MjpegFrameSource: Read failed, attempting reconnect...");
            if (!TryReconnect(lock))
            {
                return std::nullopt;
            }
            // Re-check state: Close() may have run during the reconnect sleep
            if (!isOpen || permanentlyFailed)
            {
                return std::nullopt;
            }
            if (!capture.read(image) || image.empty())
            {
                return std::nullopt;
            }
        }

        Core::Frame frame;
        frame.image = image;
        frame.metadata.sequenceNumber = frameCounter;
        frame.metadata.captureTime = std::chrono::steady_clock::now();
        frame.metadata.sourceWidth = image.cols;
        frame.metadata.sourceHeight = image.rows;

        ++frameCounter;
        return frame;
    }

    bool MjpegFrameSource::IsOpen() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return isOpen && !permanentlyFailed;
    }

    std::string MjpegFrameSource::Describe() const
    {
        return "MjpegFrameSource(" + uri + ")";
    }

    bool MjpegFrameSource::TryReconnect(std::unique_lock<std::mutex> &lock)
    {
        // Called with lock already held.
        for (int attempt = 1; attempt <= maxReconnectAttempts; ++attempt)
        {
            currentReconnectAttempts = attempt;
            LOG_WARN("MjpegFrameSource: reconnecting {}/{} to {}", attempt, maxReconnectAttempts, uri);

            capture.release();

            if (reconnectDelayMs > 0)
            {
                // Temporarily release the lock while sleeping so Close() / IsOpen() are not blocked.
                // Re-check state after reacquire — Close() may have run during the sleep.
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs));
                lock.lock();

                if (!isOpen || permanentlyFailed)
                {
                    return false;
                }
            }

            capture.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, static_cast<double>(grabTimeoutMs));
            capture.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, static_cast<double>(grabTimeoutMs));

            if (capture.open(uri))
            {
                LOG_INFO("MjpegFrameSource: reconnected to {} on attempt {}", uri, attempt);
                currentReconnectAttempts = 0;
                return true;
            }
        }

        LOG_ERROR("MjpegFrameSource: exhausted {} reconnect attempts for {}", maxReconnectAttempts, uri);
        isOpen = false;
        permanentlyFailed = true;
        return false;
    }

    std::unique_ptr<FrameSource> MjpegFrameSource::CreateMjpegFrameSource(const Core::CameraConfig &config)
    {
        return std::make_unique<MjpegFrameSource>(config);
    }

} // namespace SH3DS::Capture
