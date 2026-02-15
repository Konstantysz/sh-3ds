#include "Capture/FileFrameSource.h"
#include "Capture/FramePreprocessor.h"
#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/ConfigDrivenFSM.h"
#include "Input/MockInputAdapter.h"
#include "Pipeline/Orchestrator.h"
#include "Strategy/SoftResetStrategy.h"
#include "Vision/DominantColorDetector.h"

#include "Kappa/Logger.h"

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
    LOG_INFO("Starting main...");

    std::string hardwareConfigPath = "config/hardware.yaml";
    std::string gameProfilePath = "config/games/pokemon_xy.yaml";
    std::string huntConfigPath = "config/hunts/xy_starter_sr.yaml";
    std::string detectionProfilePath = "config/detection/xy_fennekin.yaml";

    app.add_option("--hardware", hardwareConfigPath, "Path to hardware config YAML");
    app.add_option("--game", gameProfilePath, "Path to game profile YAML");
    app.add_option("--hunt", huntConfigPath, "Path to hunt strategy config YAML");
    app.add_option("--detection", detectionProfilePath, "Path to detection profile YAML");

    CLI11_PARSE(app, argc, argv);

    try
    {
        auto hardwareConfig = SH3DS::Core::LoadHardwareConfig(hardwareConfigPath);
        auto gameProfile = SH3DS::Core::LoadGameProfile(gameProfilePath);
        auto huntConfig = SH3DS::Core::LoadHuntConfig(huntConfigPath);
        auto detectionProfile = SH3DS::Core::LoadDetectionProfile(detectionProfilePath);

        LOG_INFO("SH-3DS v{}", "0.1.0");
        LOG_INFO("Game: {}", gameProfile.gameName);
        LOG_INFO("Hunt: {} (target: {})", huntConfig.huntName, huntConfig.targetPokemon);

        auto frameSource = SH3DS::Capture::FileFrameSource::CreateFileFrameSource(
            hardwareConfig.camera.uri, hardwareConfig.orchestrator.targetFps);

        auto preprocessor =
            std::make_unique<SH3DS::Capture::FramePreprocessor>(hardwareConfig.screenCalibration, gameProfile.rois);

        auto fsm = std::make_unique<SH3DS::FSM::ConfigDrivenFSM>(gameProfile);

        std::unique_ptr<SH3DS::Vision::ShinyDetector> detector;
        if (!detectionProfile.methods.empty())
        {
            detector = SH3DS::Vision::DominantColorDetector::CreateDominantColorDetector(
                detectionProfile.methods[0], detectionProfile.profileId);
        }

        auto strategy = std::make_unique<SH3DS::Strategy::SoftResetStrategy>(huntConfig);
        auto input = SH3DS::Input::MockInputAdapter::CreateMockInputAdapter();

        SH3DS::Pipeline::Orchestrator orchestrator(std::move(frameSource),
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
