#include "App/SH3DSDebugApp.h"
#include "Kappa/Logger.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[])
{
    Kappa::Logger::SetLoggerName("SH-3DS");

#ifndef NDEBUG
    Kappa::Logger::Get().SetLevel(spdlog::level::debug);
    LOG_DEBUG("Log level set to Debug for debug build");
#endif

    CLI::App app{ "SH-3DS: Shiny Hunting Debug GUI" };

    std::string hardwareConfigPath = "config/hardware.yaml";
    std::string huntConfigPath = "config/hunts/xy_starter_sr_fennekin.yaml";
    std::string replayPath;

    app.add_option("--hardware", hardwareConfigPath, "Path to hardware config YAML");
    app.add_option("--hunt-config", huntConfigPath, "Path to unified hunt config YAML");
    app.add_option("--replay", replayPath, "Replay source (directory or video file)");

    CLI11_PARSE(app, argc, argv);

    try
    {
        SH3DS::App::SH3DSDebugApp debugApp(hardwareConfigPath, huntConfigPath, replayPath);
        debugApp.Run();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
