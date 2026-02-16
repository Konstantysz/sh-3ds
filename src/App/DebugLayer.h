#pragma once

#include "Capture/FramePreprocessor.h"
#include "Capture/FrameSeeker.h"
#include "Capture/FrameSource.h"
#include "Capture/ScreenDetector.h"
#include "Core/Constants.h"
#include "Core/Types.h"
#include "FSM/GameStateFSM.h"
#include "Kappa/Layer.h"
#include "PlaybackController.h"
#include "Vision/ShinyDetector.h"

#include <memory>
#include <optional>
#include <string>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

namespace SH3DS::App
{
    /**
     * @brief ImGui debug GUI layer for offline frame replay and pipeline visualization.
     */
    class DebugLayer : public Kappa::Layer
    {
    public:
        /**
         * @brief Constructs the debug layer.
         * @param window GLFW window handle for ImGui initialization.
         * @param source Frame source for streaming.
         * @param seeker Non-owning seek interface (same object as source).
         * @param preprocessor Frame preprocessor for perspective warp.
         * @param fsm Game state FSM.
         * @param detector Shiny detector (may be null).
         * @param totalFrames Total number of frames in the source.
         * @param targetFps Target playback FPS.
         */
        DebugLayer(GLFWwindow *window,
            std::unique_ptr<Capture::FrameSource> source,
            std::shared_ptr<Capture::FrameSeeker> seeker,
            std::unique_ptr<Capture::ScreenDetector> screenDetector,
            std::unique_ptr<Capture::FramePreprocessor> preprocessor,
            std::unique_ptr<FSM::GameStateFSM> fsm,
            std::unique_ptr<Vision::ShinyDetector> detector,
            size_t totalFrames,
            float targetFps);

        ~DebugLayer() override;

        void OnUpdate(float deltaTime) override;
        void OnRender() override;

    private:
        /**
         * @brief Processes the current frame through the pipeline.
         */
        void ProcessCurrentFrame();

        /**
         * @brief Renders an image panel with ImGui::Image.
         * @param title Panel title.
         * @param textureId OpenGL texture ID.
         * @param width Image width.
         * @param height Image height.
         */
        void RenderImagePanel(const char *title, GLuint textureId, int width, int height);

        /**
         * @brief Renders the FSM state info panel.
         */
        void RenderStatePanel();

        /**
         * @brief Renders the playback controls bar.
         */
        void RenderPlaybackControls();

        // Pipeline components
        std::unique_ptr<Capture::FrameSource> source;             ///< Frame source (streaming)
        std::shared_ptr<Capture::FrameSeeker> seeker = nullptr;   ///< Non-owning seek interface
        std::unique_ptr<Capture::ScreenDetector> screenDetector;  ///< Automatic screen detection
        std::unique_ptr<Capture::FramePreprocessor> preprocessor; ///< Perspective warp
        std::unique_ptr<FSM::GameStateFSM> fsm;                   ///< Game state FSM
        std::unique_ptr<Vision::ShinyDetector> detector;          ///< Shiny detector

        // Playback
        PlaybackController playback; ///< Playback state controller

        // OpenGL textures
        GLuint rawFrameTexture = 0;     ///< Texture for raw camera frame
        GLuint topScreenTexture = 0;    ///< Texture for warped top screen
        GLuint bottomScreenTexture = 0; ///< Texture for warped bottom screen

        // Current frame data
        cv::Mat currentRawFrame;     ///< Current raw camera frame
        cv::Mat currentTopScreen;    ///< Current warped top screen
        cv::Mat currentBottomScreen; ///< Current warped bottom screen

        // State info
        std::string currentStateName = "unknown";            ///< Current FSM state
        float timeInState = 0.0f;                            ///< Time in current state (seconds)
        std::optional<Core::ShinyResult> currentShinyResult; ///< Latest shiny detection result
        size_t lastProcessedFrame = SIZE_MAX;                ///< Last processed frame index

        // Frame dimensions (for display)
        int rawWidth = 0;       ///< Raw frame width
        int rawHeight = 0;      ///< Raw frame height
        int topWidth = Core::kTopScreenWidth;        ///< Top screen width
        int topHeight = Core::kTopScreenHeight;      ///< Top screen height
        int bottomWidth = Core::kBottomScreenWidth;   ///< Bottom screen width
        int bottomHeight = Core::kBottomScreenHeight; ///< Bottom screen height
    };
} // namespace SH3DS::App
