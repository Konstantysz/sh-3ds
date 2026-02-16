#include "PlaybackController.h"

#include <algorithm>

namespace SH3DS::App
{
    PlaybackController::PlaybackController(size_t totalFrames, float targetFps)
        : totalFrames(totalFrames),
          targetFps(targetFps)
    {
    }

    bool PlaybackController::Update(float deltaTime)
    {
        frameAdvanced = false;

        if (!playing || totalFrames == 0)
        {
            return false;
        }

        float effectiveFps = targetFps * playbackSpeed;
        if (effectiveFps <= 0.0f)
        {
            return false;
        }

        float frameInterval = 1.0f / effectiveFps;
        accumulatedTime += deltaTime;

        if (accumulatedTime >= frameInterval)
        {
            accumulatedTime -= frameInterval;

            if (currentFrameIndex + 1 < totalFrames)
            {
                ++currentFrameIndex;
                frameAdvanced = true;
            }
            else
            {
                playing = false;
            }
        }

        return frameAdvanced;
    }

    void PlaybackController::Play()
    {
        if (currentFrameIndex + 1 >= totalFrames)
        {
            currentFrameIndex = 0;
        }
        playing = true;
        accumulatedTime = 0.0f;
    }

    void PlaybackController::Pause()
    {
        playing = false;
    }

    void PlaybackController::TogglePlayPause()
    {
        if (playing)
        {
            Pause();
        }
        else
        {
            Play();
        }
    }

    void PlaybackController::StepForward()
    {
        playing = false;
        if (currentFrameIndex + 1 < totalFrames)
        {
            ++currentFrameIndex;
            frameAdvanced = true;
        }
    }

    void PlaybackController::StepBackward()
    {
        playing = false;
        if (currentFrameIndex > 0)
        {
            --currentFrameIndex;
            frameAdvanced = true;
        }
    }

    void PlaybackController::SetFrameIndex(size_t index)
    {
        currentFrameIndex = std::min(index, totalFrames > 0 ? totalFrames - 1 : 0);
        frameAdvanced = true;
    }

    void PlaybackController::SetPlaybackSpeed(float speed)
    {
        playbackSpeed = std::max(0.1f, std::min(speed, 10.0f));
    }

    size_t PlaybackController::GetCurrentFrameIndex() const
    {
        return currentFrameIndex;
    }

    size_t PlaybackController::GetTotalFrames() const
    {
        return totalFrames;
    }

    bool PlaybackController::IsPlaying() const
    {
        return playing;
    }

    float PlaybackController::GetPlaybackSpeed() const
    {
        return playbackSpeed;
    }

    float PlaybackController::GetTargetFps() const
    {
        return targetFps;
    }

} // namespace SH3DS::App
