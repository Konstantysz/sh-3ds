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
            LOG_ERROR("Orchestrator: Invalid target FPS ({:.1f}). Clamping to 30.0", config.targetFps);
            config.targetFps = 30.0;
        }

        running = true;
        auto tickInterval = std::chrono::microseconds(static_cast<int64_t>(1'000'000.0 / config.targetFps));

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
                auto tickStart = std::chrono::steady_clock::now();

                MainLoopTick();

                auto tickEnd = std::chrono::steady_clock::now();
                auto elapsed = tickEnd - tickStart;
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

        LOG_INFO("Orchestrator stopped. Final stats: {} encounters, {} shinies",
            strategy->Stats().encounters,
            strategy->Stats().shiniesFound);
    }

    void Orchestrator::Stop()
    {
        running = false;
    }

    Core::HuntStatistics Orchestrator::Stats() const
    {
        return strategy->Stats();
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

        // Auto-detect screen corners and update preprocessor
        if (screenDetector)
        {
            screenDetector->ApplyTo(*preprocessor, frame->image);
        }

        LOG_DEBUG("Orchestrator: Processing frame #{}...", frame->metadata.sequenceNumber);
        auto rois = preprocessor->Process(frame->image);
        if (!rois.has_value())
        {
            LOG_DEBUG("Orchestrator: Screen not detected in frame #{}", frame->metadata.sequenceNumber);
            return;
        }

        LOG_DEBUG("Orchestrator: Updating FSM...");
        auto transition = fsm->Update(*rois);
        if (transition.has_value())
        {
            LOG_INFO(
                "Frame #{}: FSM Transition {} -> {}", frame->metadata.sequenceNumber, transition->from, transition->to);
        }

        LOG_DEBUG("Orchestrator: Detecting shiny...");
        std::optional<Core::ShinyResult> shinyResult;
        auto spriteIt = rois->find("pokemon_sprite");
        if (spriteIt != rois->end() && !spriteIt->second.empty())
        {
            shinyResult = detector->Detect(spriteIt->second);
        }

        LOG_DEBUG("Orchestrator: Strategy tick (current state: {})...", fsm->CurrentState());
        auto strategyDecision = strategy->Tick(fsm->CurrentState(), fsm->TimeInCurrentState(), shinyResult);

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
            LOG_WARN(
                "Watchdog: FSM stuck in state '{}' for {}ms", fsm->CurrentState(), fsm->TimeInCurrentState().count());

            auto recovery = strategy->OnStuck();
            ExecuteDecision(recovery);

            if (recovery.decision.action == Core::HuntAction::Abort)
            {
                Stop();
            }
            else
            {
                fsm->ForceState("unknown");
                detector->Reset();
            }
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
