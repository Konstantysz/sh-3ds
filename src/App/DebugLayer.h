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

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

namespace SH3DS::App
{
    /**
     * @brief ImGui debug GUI layer for offline frame replay and live camera pipeline visualization.
     */
    class DebugLayer : public Kappa::Layer
    {
    public:
        /**
         * @brief Constructs the debug layer.
         * @param window GLFW window handle for ImGui initialization.
         * @param source Frame source for streaming.
         * @param seeker Non-owning seek interface — nullptr signals live (non-seekable) mode.
         * @param screenDetector Automatic screen corner detector.
         * @param preprocessor Frame preprocessor for perspective warp.
         * @param fsm Game state FSM.
         * @param detector Shiny detector (may be null).
         * @param shinyRoi ROI name for shiny detection.
         * @param shinyCheckState FSM state in which shiny detection runs.
         * @param totalFrames Total number of frames in replay source (0 for live).
         * @param targetFps Target playback FPS.
         */
        DebugLayer(GLFWwindow *window,
            std::unique_ptr<Capture::FrameSource> source,
            std::shared_ptr<Capture::FrameSeeker> seeker,
            std::unique_ptr<Capture::ScreenDetector> screenDetector,
            std::unique_ptr<Capture::FramePreprocessor> preprocessor,
            std::unique_ptr<FSM::GameStateFSM> fsm,
            std::unique_ptr<Vision::ShinyDetector> detector,
            std::string shinyRoi,
            std::string shinyCheckState,
            size_t totalFrames,
            float targetFps);

        ~DebugLayer() override;

        void OnUpdate(float deltaTime) override;

        void OnRender() override;

    private:
        /**
         * @brief Processes a single frame through the full pipeline (screen detection, warp, FSM, detector).
         * @param frame Frame to process.
         */
        void ProcessFrame(const Core::Frame &frame);

        /**
         * @brief Seeks to the playback controller's current index, grabs, and processes (replay only).
         */
        void GrabCurrentReplayFrame();

        /**
         * @brief Starts the background capture thread (live mode only).
         */
        void StartCaptureThread();

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
         * @brief Renders the playback controls bar (scrubber in replay mode, LIVE badge in live mode).
         */
        void RenderPlaybackControls();

        // Pipeline components
        std::unique_ptr<Capture::FrameSource> source;             ///< Frame source (streaming)
        std::shared_ptr<Capture::FrameSeeker> seeker = nullptr;   ///< Non-owning seek interface; nullptr = live
        std::unique_ptr<Capture::ScreenDetector> screenDetector;  ///< Automatic screen detection
        std::unique_ptr<Capture::FramePreprocessor> preprocessor; ///< Perspective warp
        std::unique_ptr<FSM::GameStateFSM> fsm;                   ///< Game state FSM
        std::unique_ptr<Vision::ShinyDetector> detector;          ///< Shiny detector
        std::string shinyRoi;                                     ///< ROI name for shiny detection
        std::string shinyCheckState;                              ///< FSM state in which shiny detection runs

        // Playback (replay mode)
        PlaybackController playback; ///< Playback state controller

        // Live mode
        bool isLiveSource = false;                         ///< True when seeker == nullptr (live camera)
        std::thread captureThread;                         ///< Background capture thread (live only)
        std::atomic<bool> captureRunning{ false };         ///< Signals capture thread to stop
        std::mutex latestFrameMutex;                       ///< Guards latestFrame
        std::optional<Core::Frame> latestFrame;            ///< Latest frame from capture thread
        std::atomic<size_t> totalFramesGrabbed{ 0 };       ///< Monotonically increasing grab counter
        float liveGrabFps = 0.0f;                          ///< Estimated grab FPS (render thread)
        std::chrono::steady_clock::time_point lastFpsTime; ///< Timestamp for FPS estimation
        size_t lastFpsCount = 0;                           ///< totalFramesGrabbed at last FPS sample

        // OpenGL textures
        GLuint rawFrameTexture = 0;     ///< Texture for raw camera frame
        GLuint topScreenTexture = 0;    ///< Texture for warped top screen
        GLuint bottomScreenTexture = 0; ///< Texture for warped bottom screen

        // Current frame data
        cv::Mat currentRawFrame;     ///< Current raw camera frame
        cv::Mat currentTopScreen;    ///< Current warped top screen
        cv::Mat currentBottomScreen; ///< Current warped bottom screen

        // Display options
        bool applyColorImprovementToDisplay = false; ///< Whether to apply color correction to displayed warped frames

        // State info
        std::string currentStateName = "unknown";            ///< Current FSM state
        float timeInState = 0.0f;                            ///< Time in current state (seconds)
        std::optional<Core::ShinyResult> currentShinyResult; ///< Latest shiny detection result
        size_t lastProcessedFrame = SIZE_MAX;                ///< Last processed frame index (replay only)

        // Frame dimensions (for display)
        int rawWidth = 0;                             ///< Raw frame width
        int rawHeight = 0;                            ///< Raw frame height
        int topWidth = Core::kTopScreenWidth;         ///< Top screen width
        int topHeight = Core::kTopScreenHeight;       ///< Top screen height
        int bottomWidth = Core::kBottomScreenWidth;   ///< Bottom screen width
        int bottomHeight = Core::kBottomScreenHeight; ///< Bottom screen height
    };
} // namespace SH3DS::App
