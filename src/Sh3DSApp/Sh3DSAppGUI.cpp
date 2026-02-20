#include "App/SH3DSDebugApp.h"
#include "Capture/FileFrameSource.h"
#include "Capture/FramePreprocessor.h"
#include "Capture/ScreenDetector.h"
#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/HuntProfiles.h"
#include "Input/MockInputAdapter.h"
#include "Kappa/Logger.h"
#include "Pipeline/Orchestrator.h"
#include "Strategy/SoftResetStrategy.h"
#include "Vision/DominantColorDetector.h"

#include <CLI/CLI.hpp>

#include <csignal>
#include <memory>

static SH3DS::Pipeline::Orchestrator *globalOrchestrator = nullptr;

void SignalHandler([[maybe_unused]] int signal)
{
    if (globalOrchestrator)
    {
        globalOrchestrator->Stop();
    }
}

int main(int argc, char *argv[])
{
    // Set logger name before any logging
    Kappa::Logger::SetLoggerName("SH-3DS");

    CLI::App app{ "SH-3DS: Networked Shiny Hunting Bot" };

    std::string mode = "console";
    std::string hardwareConfigPath = "config/hardware.yaml";
    std::string huntConfigPath = "config/hunts/xy_starter_sr_fennekin.yaml";
    std::string replayPath;

    // TODO: split GUI into separate sh3ds-debug executable to avoid linking ImGui/OpenGL in console builds
    app.add_option("--mode", mode, "Mode: console or gui")
        ->default_val("console")
        ->check(CLI::IsMember({ "console", "gui" }));
    app.add_option("--hardware", hardwareConfigPath, "Path to hardware config YAML");
    app.add_option("--hunt-config", huntConfigPath, "Path to unified hunt config YAML");
    app.add_option("--replay", replayPath, "Replay source (directory or video file) for GUI mode");

    CLI11_PARSE(app, argc, argv);

    try
    {
        if (mode == "gui")
        {
            if (replayPath.empty())
            {
                LOG_ERROR("--replay is required for GUI mode");
                return 1;
            }

            LOG_INFO("Starting GUI mode...");
            SH3DS::App::SH3DSDebugApp debugApp(hardwareConfigPath, huntConfigPath, replayPath);
            debugApp.Run();
            return 0;
        }

        // Console mode (default)
        LOG_INFO("Starting console mode...");

        auto hardwareConfig = SH3DS::Core::LoadHardwareConfig(hardwareConfigPath);
        auto unifiedConfig = SH3DS::Core::LoadUnifiedHuntConfig(huntConfigPath);

        LOG_INFO("SH-3DS v{}", "0.1.0");
        LOG_INFO("Hunt: {} (target: {})", unifiedConfig.huntName, unifiedConfig.targetPokemon);

        auto frameSource = SH3DS::Capture::FileFrameSource::CreateFileFrameSource(
            hardwareConfig.camera.uri, hardwareConfig.orchestrator.targetFps);

        auto screenDetector = SH3DS::Capture::ScreenDetector::CreateScreenDetector();

        auto preprocessor = std::make_unique<SH3DS::Capture::FramePreprocessor>(
            hardwareConfig.screenCalibration, unifiedConfig.rois, hardwareConfig.bottomScreenCalibration);

        // TODO(hunt-profile-dispatch): Currently only XY Starter SR is supported. When multi-profile
        // support is needed, add a `hunt_profile` key to the unified hunt YAML and dispatch here via
        // a lookup table (e.g. "xy_starter_sr" -> HuntProfiles::CreateXYStarterSR).
        auto fsm = SH3DS::FSM::HuntProfiles::CreateXYStarterSR(unifiedConfig.fsmParams);

        std::unique_ptr<SH3DS::Vision::ShinyDetector> detector;
        if (!unifiedConfig.shinyDetector.method.empty())
        {
            detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(
                unifiedConfig.shinyDetector, unifiedConfig.huntId);
        }

        auto strategy = std::make_unique<SH3DS::Strategy::SoftResetStrategy>(SH3DS::Core::ToHuntConfig(unifiedConfig));
        auto input = SH3DS::Input::MockInputAdapter::CreateMockInputAdapter();

        // Route the configured shiny ROI from the hunt config into the orchestrator.
        // TODO(hunt-profile-dispatch): When multi-profile support is added, derive this from
        // the hunt profile factory rather than reading it directly from UnifiedHuntConfig.
        if (!unifiedConfig.shinyDetector.roi.empty())
        {
            hardwareConfig.orchestrator.shinyRoi = unifiedConfig.shinyDetector.roi;
        }

        SH3DS::Pipeline::Orchestrator orchestrator(std::move(frameSource),
            std::move(screenDetector),
            std::move(preprocessor),
            std::move(fsm),
            std::move(detector),
            std::move(strategy),
            std::move(input),
            hardwareConfig.orchestrator);

        globalOrchestrator = &orchestrator;
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        orchestrator.Run();

        auto stats = orchestrator.Stats();
        LOG_INFO("Final: {} encounters, {} shinies, avg cycle: {:.1f}s",
            stats.encounters,
            stats.shiniesFound,
            stats.avgCycleSeconds);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
