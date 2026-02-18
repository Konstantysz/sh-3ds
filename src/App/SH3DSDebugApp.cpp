#include "SH3DSDebugApp.h"

#include "Capture/FileFrameSource.h"
#include "Capture/VideoFrameSource.h"
#include "Core/Config.h"
#include "DebugLayer.h"
#include "FSM/HuntProfiles.h"
#include "Kappa/Logger.h"
#include "Vision/DominantColorDetector.h"

#include <filesystem>

namespace SH3DS::App
{
    SH3DSDebugApp::SH3DSDebugApp(const std::string &hardwareConfigPath,
        const std::string &huntConfigPath,
        const std::string &replaySourcePath)
        : Application(GetSpec())
    {
        auto pipeline = BuildPipeline(hardwareConfigPath, huntConfigPath, replaySourcePath);

        GLFWwindow *windowHandle = GetWindow().GetHandle();

        PushLayer<DebugLayer>(windowHandle,
            std::move(pipeline.source),
            pipeline.seeker,
            std::move(pipeline.screenDetector),
            std::move(pipeline.preprocessor),
            std::move(pipeline.fsm),
            std::move(pipeline.detector),
            pipeline.shinyRoi,
            pipeline.shinyCheckState,
            pipeline.totalFrames,
            pipeline.targetFps);
    }

    SH3DSDebugApp::PipelineComponents SH3DSDebugApp::BuildPipeline(const std::string &hardwareConfigPath,
        const std::string &huntConfigPath,
        const std::string &replaySourcePath)
    {
        auto hardwareConfig = Core::LoadHardwareConfig(hardwareConfigPath);
        auto unifiedConfig = Core::LoadUnifiedHuntConfig(huntConfigPath);

        LOG_INFO("SH-3DS Debug GUI");
        LOG_INFO("Hunt: {} (target: {})", unifiedConfig.huntName, unifiedConfig.targetPokemon);
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
            hardwareConfig.screenCalibration, unifiedConfig.rois, hardwareConfig.bottomScreenCalibration);

        // Create FSM
        pipeline.fsm = FSM::HuntProfiles::CreateXYStarterSR(unifiedConfig.fsmParams);

        // Create detector
        if (!unifiedConfig.shinyDetector.method.empty())
        {
            pipeline.detector = Vision::DominantColorDetector::CreateDominantColorDetector(
                unifiedConfig.shinyDetector, unifiedConfig.huntId);
            pipeline.shinyRoi = unifiedConfig.shinyDetector.roi;
            pipeline.shinyCheckState = unifiedConfig.shinyCheckState;
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
