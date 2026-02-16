#pragma once

#include <cstddef>

namespace SH3DS::App
{
    /**
     * @brief Manages playback state for offline frame replay.
     */
    class PlaybackController
    {
    public:
        /**
         * @brief Constructs a PlaybackController.
         * @param totalFrames Total number of frames available.
         * @param targetFps Target playback FPS.
         */
        PlaybackController(size_t totalFrames, float targetFps);

        /**
         * @brief Updates playback state. Call once per GUI frame.
         * @param deltaTime Time since last update in seconds.
         * @return True if a new frame should be processed.
         */
        bool Update(float deltaTime);

        /** @brief Start playback. */
        void Play();

        /** @brief Pause playback. */
        void Pause();

        /** @brief Toggle between play and pause. */
        void TogglePlayPause();

        /** @brief Advance one frame (pauses playback). */
        void StepForward();

        /** @brief Go back one frame (pauses playback). */
        void StepBackward();

        /**
         * @brief Jump to a specific frame index.
         * @param index Zero-based frame index.
         */
        void SetFrameIndex(size_t index);

        /**
         * @brief Set playback speed multiplier.
         * @param speed Speed multiplier (1.0 = normal).
         */
        void SetPlaybackSpeed(float speed);

        /** @brief Returns current frame index. */
        [[nodiscard]] size_t GetCurrentFrameIndex() const;

        /** @brief Returns total number of frames. */
        [[nodiscard]] size_t GetTotalFrames() const;

        /** @brief Returns true if currently playing. */
        [[nodiscard]] bool IsPlaying() const;

        /** @brief Returns current playback speed multiplier. */
        [[nodiscard]] float GetPlaybackSpeed() const;

        /** @brief Returns target FPS. */
        [[nodiscard]] float GetTargetFps() const;

    private:
        size_t currentFrameIndex = 0; ///< Current frame position
        size_t totalFrames;           ///< Total available frames
        float targetFps;              ///< Base playback FPS
        float playbackSpeed = 1.0f;   ///< Speed multiplier
        float accumulatedTime = 0.0f; ///< Time accumulator for frame timing
        bool playing = false;         ///< Whether playback is active
        bool frameAdvanced = false;   ///< Whether frame changed this update
    };
} // namespace SH3DS::App
