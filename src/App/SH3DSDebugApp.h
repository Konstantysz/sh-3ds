#pragma once

#include "Capture/FramePreprocessor.h"
#include "Capture/FrameSeeker.h"
#include "Capture/FrameSource.h"
#include "Capture/ScreenDetector.h"
#include "FSM/GameStateFSM.h"
#include "Kappa/Application.h"
#include "Vision/ShinyDetector.h"

#include <memory>
#include <string>

namespace SH3DS::App
{
    /**
     * @brief Debug application with ImGui GUI for offline replay.
     */
    class SH3DSDebugApp : public Kappa::Application
    {
    public:
        /**
         * @brief Constructs the debug application.
         * @param hardwareConfigPath Path to hardware config YAML.
         * @param gameProfilePath Path to game profile YAML.
         * @param detectionProfilePath Path to detection profile YAML.
         * @param replaySourcePath Path to replay source (directory or video file).
         */
        SH3DSDebugApp(const std::string &hardwareConfigPath,
            const std::string &gameProfilePath,
            const std::string &detectionProfilePath,
            const std::string &replaySourcePath);

    private:
        /**
         * @brief Pipeline components built before GUI initialization.
         */
        struct PipelineComponents
        {
            std::unique_ptr<Capture::FrameSource> source;             ///< Frame source (streaming)
            std::shared_ptr<Capture::FrameSeeker> seeker = nullptr;   ///< Non-owning seek interface
            std::unique_ptr<Capture::ScreenDetector> screenDetector;  ///< Automatic screen detection
            std::unique_ptr<Capture::FramePreprocessor> preprocessor; ///< Perspective warp
            std::unique_ptr<FSM::GameStateFSM> fsm;                   ///< Game state FSM
            std::unique_ptr<Vision::ShinyDetector> detector;          ///< Shiny detector (may be null)
            size_t totalFrames = 0;                                   ///< Total frames in source
            float targetFps = 12.0f;                                  ///< Target playback FPS
        };

        /**
         * @brief Loads configs and creates all pipeline components.
         * @param hardwareConfigPath Path to hardware config YAML.
         * @param gameProfilePath Path to game profile YAML.
         * @param detectionProfilePath Path to detection profile YAML.
         * @param replaySourcePath Path to replay source (directory or video file).
         * @return Assembled pipeline components.
         */
        static PipelineComponents BuildPipeline(const std::string &hardwareConfigPath,
            const std::string &gameProfilePath,
            const std::string &detectionProfilePath,
            const std::string &replaySourcePath);

        /**
         * @brief Returns the application specification.
         * @return Application specification.
         */
        static Kappa::ApplicationSpecification GetSpec();
    };
} // namespace SH3DS::App
