#include "SH3DSDebugApp.h"

#include "Capture/FileFrameSource.h"
#include "Capture/VideoFrameSource.h"
#include "Core/Config.h"
#include "DebugLayer.h"
#include "FSM/ConfigDrivenFSM.h"
#include "Kappa/Logger.h"
#include "Vision/DominantColorDetector.h"

#include <filesystem>

namespace SH3DS::App
{
    SH3DSDebugApp::SH3DSDebugApp(const std::string &hardwareConfigPath,
        const std::string &gameProfilePath,
        const std::string &detectionProfilePath,
        const std::string &replaySourcePath)
        : Application(GetSpec())
    {
        auto pipeline = BuildPipeline(hardwareConfigPath, gameProfilePath, detectionProfilePath, replaySourcePath);

        GLFWwindow *windowHandle = GetWindow().GetHandle();

        PushLayer<DebugLayer>(windowHandle,
            std::move(pipeline.source),
            pipeline.seeker,
            std::move(pipeline.screenDetector),
            std::move(pipeline.preprocessor),
            std::move(pipeline.fsm),
            std::move(pipeline.detector),
            pipeline.totalFrames,
            pipeline.targetFps);
    }

    SH3DSDebugApp::PipelineComponents SH3DSDebugApp::BuildPipeline(const std::string &hardwareConfigPath,
        const std::string &gameProfilePath,
        const std::string &detectionProfilePath,
        const std::string &replaySourcePath)
    {
        auto hardwareConfig = Core::LoadHardwareConfig(hardwareConfigPath);
        auto gameProfile = Core::LoadGameProfile(gameProfilePath);
        auto detectionProfile = Core::LoadDetectionProfile(detectionProfilePath);

        LOG_INFO("SH-3DS Debug GUI");
        LOG_INFO("Game: {}", gameProfile.gameName);
        LOG_INFO("Replay: {}", replaySourcePath);

        PipelineComponents pipeline;

        // Create frame source
        std::filesystem::path sourcePath(replaySourcePath);

        if (std::filesystem::is_directory(sourcePath))
        {
            auto fileSource =
                std::make_unique<Capture::FileFrameSource>(sourcePath, hardwareConfig.orchestrator.targetFps);
            fileSource->Open();
            pipeline.totalFrames = fileSource->GetFrameCount();
            pipeline.seeker = std::shared_ptr<Capture::FrameSeeker>(fileSource.get());
            LOG_INFO("Source: directory ({} frames)", pipeline.totalFrames);
            pipeline.source = std::move(fileSource);
        }
        else
        {
            auto videoSource =
                std::make_unique<Capture::VideoFrameSource>(sourcePath, hardwareConfig.orchestrator.targetFps);
            videoSource->Open();
            pipeline.totalFrames = videoSource->GetFrameCount();
            pipeline.seeker = std::shared_ptr<Capture::FrameSeeker>(videoSource.get());
            LOG_INFO("Source: video ({} frames)", pipeline.totalFrames);
            pipeline.source = std::move(videoSource);
        }

        // Create screen detector for automatic corner detection
        pipeline.screenDetector = Capture::ScreenDetector::CreateScreenDetector();

        // Create preprocessor with optional bottom screen (corners set by ScreenDetector)
        pipeline.preprocessor = std::make_unique<Capture::FramePreprocessor>(
            hardwareConfig.screenCalibration, gameProfile.rois, hardwareConfig.bottomScreenCalibration);

        // Create FSM
        pipeline.fsm = std::make_unique<FSM::ConfigDrivenFSM>(gameProfile);

        // Create detector
        if (!detectionProfile.methods.empty())
        {
            pipeline.detector = Vision::DominantColorDetector::CreateDominantColorDetector(
                detectionProfile.methods[0], detectionProfile.profileId);
        }

        pipeline.targetFps = static_cast<float>(hardwareConfig.orchestrator.targetFps);

        return pipeline;
    }

    Kappa::ApplicationSpecification SH3DSDebugApp::GetSpec()
    {
        Kappa::ApplicationSpecification spec;
        spec.name = "SH-3DS Debug";
        spec.windowSpecification.title = "SH-3DS Debug - Offline Replay";
        spec.windowSpecification.width = 1600;
        spec.windowSpecification.height = 900;
        return spec;
    }

} // namespace SH3DS::App
