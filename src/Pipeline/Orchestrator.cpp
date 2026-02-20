#include "Orchestrator.h"

#include "Kappa/Logger.h"

#include <chrono>
#include <thread>

namespace SH3DS::Pipeline
{
    Orchestrator::Orchestrator(std::unique_ptr<Capture::FrameSource> frameSource,
        std::unique_ptr<Capture::ScreenDetector> screenDetector,
        std::unique_ptr<Capture::FramePreprocessor> preprocessor,
        std::unique_ptr<FSM::GameStateFSM> fsm,
        std::unique_ptr<Vision::ShinyDetector> detector,
        std::unique_ptr<Strategy::HuntStrategy> strategy,
        std::unique_ptr<Input::InputAdapter> input,
        Core::OrchestratorConfig config)
        : frameSource(std::move(frameSource)),
          screenDetector(std::move(screenDetector)),
          preprocessor(std::move(preprocessor)),
          fsm(std::move(fsm)),
          detector(std::move(detector)),
          strategy(std::move(strategy)),
          input(std::move(input)),
          config(std::move(config))
    {
    }

    void Orchestrator::Run()
    {
        if (config.targetFps <= 0.0)
        {
            LOG_WARN("Orchestrator: Invalid target FPS ({:.1f}). Clamping to 30.0", config.targetFps);
            config.targetFps = 30.0;
        }

        running = true;
        const auto tickInterval = std::chrono::microseconds(static_cast<int64_t>(1'000'000.0 / config.targetFps));

        LOG_INFO("Orchestrator starting at {:.1f} FPS (dry_run={})", config.targetFps, config.dryRun);

        if (frameSource && !frameSource->Open())
        {
            LOG_CRITICAL("Orchestrator: Failed to open frame source: {}", frameSource->Describe());
            return;
        }

        if (input && !input->IsConnected())
        {
            LOG_INFO("Orchestrator: Connecting to input adapter...");

            // Use a default address if not connected, though it should ideally be handled earlier
            if (!input->Connect("127.0.0.1"))
            {
                LOG_WARN("Orchestrator: Failed to connect to input adapter.");
            }
        }

        try
        {
            while (running)
            {
                const auto tickStart = std::chrono::steady_clock::now();

                MainLoopTick();

                const auto tickEnd = std::chrono::steady_clock::now();
                const auto elapsed = tickEnd - tickStart;
                if (elapsed < tickInterval)
                {
                    std::this_thread::sleep_for(tickInterval - elapsed);
                }
            }
        }
        catch (const std::exception &e)
        {
            LOG_CRITICAL("Orchestrator encountered a fatal exception: {}", e.what());
            running = false;
        }
        catch (...)
        {
            LOG_CRITICAL("Orchestrator encountered an unknown fatal error!");
            running = false;
        }

        // Cleanup: Release all buttons
        if (input && input->IsConnected())
        {
            try
            {
                input->ReleaseAll();
            }
            catch (...)
            {
                LOG_WARN("Failed to release buttons during shutdown");
            }
        }

        const auto finalStats = Stats();
        LOG_INFO("Orchestrator stopped. Final stats: {} encounters, {} shinies, {} watchdog stuck events",
            finalStats.encounters,
            finalStats.shiniesFound,
            finalStats.watchdogRecoveries);
    }

    void Orchestrator::Stop()
    {
        running = false;
    }

    Core::HuntStatistics Orchestrator::Stats() const
    {
        auto stats = strategy->Stats();
        stats.watchdogRecoveries += watchdogStuckCount;
        return stats;
    }

    void Orchestrator::MainLoopTick()
    {
        LOG_DEBUG("Orchestrator: Grabbing frame...");

        auto frame = frameSource->Grab();
        if (!frame.has_value())
        {
            LOG_TRACE("Orchestrator: frameSource->Grab() returned nullopt (exhausted or timeout).");
            return;
        }

        if (screenDetector)
        {
            screenDetector->ApplyTo(*preprocessor, frame->image);
        }

        LOG_DEBUG("Orchestrator: Processing frame #{}...", frame->metadata.sequenceNumber);

        auto dualScreenResult = preprocessor->ProcessDualScreen(frame->image);
        if (!dualScreenResult.has_value())
        {
            LOG_DEBUG("Orchestrator: Screen not detected in frame #{}", frame->metadata.sequenceNumber);
            return;
        }

        LOG_DEBUG("Orchestrator: Updating FSM...");

        auto transition = fsm->Update(dualScreenResult->topRois, dualScreenResult->bottomRois);
        if (transition.has_value())
        {
            LOG_INFO(
                "Frame #{}: FSM Transition {} -> {}", frame->metadata.sequenceNumber, transition->from, transition->to);
        }

        LOG_DEBUG("Orchestrator: Detecting shiny...");

        std::optional<Core::ShinyResult> shinyResult;
        if (detector)
        {
            auto spriteIt = dualScreenResult->topRois.find(config.shinyRoi);
            if (spriteIt != dualScreenResult->topRois.end() && !spriteIt->second.empty())
            {
                shinyResult = detector->Detect(spriteIt->second);
            }
        }

        LOG_DEBUG("Orchestrator: Strategy tick (current state: {})...", fsm->GetCurrentState());

        const auto strategyDecision = strategy->Tick(fsm->GetCurrentState(), fsm->GetTimeInCurrentState(), shinyResult);

        LOG_DEBUG("Orchestrator: Executing decision...");

        ExecuteDecision(strategyDecision);

        LOG_DEBUG("Orchestrator: Watchdog handling...");

        HandleWatchdog();

        LOG_DEBUG("Orchestrator: MainLoopTick complete.");
    }

    void Orchestrator::HandleWatchdog()
    {
        if (fsm->IsStuck())
        {
            ++watchdogStuckCount;
            LOG_WARN("Watchdog: FSM stuck in state '{}' for {}ms",
                fsm->GetCurrentState(),
                fsm->GetTimeInCurrentState().count());
            LOG_ERROR("ABORT: watchdog detected stuck FSM state");
            Stop();
        }
    }

    void Orchestrator::ExecuteDecision(const Strategy::StrategyDecision &strategyDecision)
    {
        const auto &decision = strategyDecision.decision;
        const auto &command = strategyDecision.command;

        switch (decision.action)
        {
        case Core::HuntAction::SendInput:
            if (!config.dryRun && input && input->IsConnected())
            {
                input->Send(command);
                if (decision.delay.count() > 0)
                {
                    std::this_thread::sleep_for(decision.delay);
                    input->ReleaseAll();
                }
            }
            LOG_DEBUG("Input: {} (buttons=0x{:04X})", decision.reason, command.buttonsPressed);
            break;

        case Core::HuntAction::AlertShiny:
            LOG_ERROR("*** SHINY FOUND! *** {}", decision.reason);
            Stop();
            break;

        case Core::HuntAction::Abort:
            LOG_ERROR("ABORT: {}", decision.reason);
            Stop();
            break;

        case Core::HuntAction::CheckShiny:
        case Core::HuntAction::Wait:
        case Core::HuntAction::Reset:
            break;
        }
    }
} // namespace SH3DS::Pipeline

