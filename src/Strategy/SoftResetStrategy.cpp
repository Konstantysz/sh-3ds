#include "SoftResetStrategy.h"

#include "Kappa/Logger.h"
#include "Input/InputCommand.h"

namespace SH3DS::Strategy
{
    SoftResetStrategy::SoftResetStrategy(Core::HuntConfig config) : config(std::move(config))
    {
        Reset();
    }

    StrategyDecision SoftResetStrategy::Tick(const Core::GameState &currentState,
        std::chrono::milliseconds timeInState,
        const std::optional<Core::ShinyResult> &shinyResult)
    {
        // State changed — reset action index
        if (currentState != lastState)
        {
            lastState = currentState;
            actionIndex = 0;
            waitingForShinyCheck = false;
            consecutiveStuckCount = 0;
        }

        // Check shiny result in the shiny check state
        if (currentState == config.shinyCheckState)
        {
            // Wait for the delay before checking
            if (timeInState < std::chrono::milliseconds(config.shinyCheckDelayMs))
            {
                return { { .action = Core::HuntAction::Wait, .reason = "waiting for shiny check delay" }, {} };
            }

            if (shinyResult.has_value())
            {
                if (shinyResult->verdict == Core::ShinyVerdict::Shiny)
                {
                    ++stats.shiniesFound;
                    LOG_ERROR("SHINY FOUND! confidence={:.3f} method={}", shinyResult->confidence, shinyResult->method);
                    return {
                        { .action = Core::HuntAction::AlertShiny, .reason = "Shiny detected! " + shinyResult->details },
                        {},
                    };
                }

                if (shinyResult->verdict == Core::ShinyVerdict::NotShiny)
                {
                    ++stats.encounters;
                    auto now = std::chrono::steady_clock::now();
                    if (stats.encounters > 1)
                    {
                        auto elapsed = std::chrono::duration<double>(now - stats.huntStarted).count();
                        stats.avgCycleSeconds = elapsed / static_cast<double>(stats.encounters);
                    }
                    stats.lastEncounter = now;

                    LOG_INFO("Encounter #{}: not shiny (confidence={:.3f})", stats.encounters, shinyResult->confidence);

                    // Continue to post-reveal / soft reset
                    waitingForShinyCheck = false;
                }
                // Uncertain — keep checking
                else
                {
                    return { { .action = Core::HuntAction::CheckShiny, .reason = "uncertain verdict, re-checking" },
                        {} };
                }
            }
            else
            {
                return { { .action = Core::HuntAction::CheckShiny, .reason = "requesting shiny check" }, {} };
            }
        }

        // Look up actions for the current state
        auto it = config.actions.find(currentState);
        if (it == config.actions.end() || it->second.empty())
        {
            return { { .action = Core::HuntAction::Wait, .reason = "no actions for state: " + currentState }, {} };
        }

        const auto &stateActions = it->second;

        // Standalone wait action
        if (actionIndex < static_cast<int>(stateActions.size()))
        {
            const auto &action = stateActions[actionIndex];

            if (action.waitMs > 0 && action.buttons.empty())
            {
                if (timeInState < std::chrono::milliseconds(action.waitMs))
                {
                    return {
                        { .action = Core::HuntAction::Wait,
                            .reason = "waiting " + std::to_string(action.waitMs) + "ms" },
                        {},
                    };
                }
                ++actionIndex;
            }
        }

        // Execute button actions
        if (actionIndex < static_cast<int>(stateActions.size()))
        {
            const auto &action = stateActions[actionIndex];

            if (!action.buttons.empty())
            {
                auto now = std::chrono::steady_clock::now();
                auto timeSinceLastAction = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastActionTime);

                if (timeSinceLastAction >= std::chrono::milliseconds(action.waitAfterMs))
                {
                    lastActionTime = now;

                    Input::InputCommand cmd = BuildInputCommand(action.buttons);

                    if (!action.repeat)
                    {
                        ++actionIndex;
                    }

                    return {
                        { .action = Core::HuntAction::SendInput,
                            .reason = "pressing buttons for state: " + currentState,
                            .delay = std::chrono::milliseconds(action.holdMs) },
                        cmd,
                    };
                }
            }
        }

        return { { .action = Core::HuntAction::Wait, .reason = "waiting for next action window" }, {} };
    }

    StrategyDecision SoftResetStrategy::OnStuck()
    {
        ++stats.watchdogRecoveries;
        ++consecutiveStuckCount;

        LOG_WARN("Strategy: stuck recovery #{} (consecutive: {})", stats.watchdogRecoveries, consecutiveStuckCount);

        if (consecutiveStuckCount > config.onStuck.maxRetries)
        {
            LOG_ERROR("Too many consecutive stuck recoveries. Aborting.");
            return { { .action = Core::HuntAction::Abort, .reason = "exceeded max stuck recoveries" }, {} };
        }

        // Force a soft reset
        Input::InputCommand cmd = BuildInputCommand({ "L", "R", "START" });
        return {
            { .action = Core::HuntAction::SendInput,
                .reason = "stuck recovery: forcing soft reset",
                .delay = std::chrono::milliseconds(500) },
            cmd,
        };
    }

    const Core::HuntStatistics &SoftResetStrategy::Stats() const
    {
        return stats;
    }

    void SoftResetStrategy::Reset()
    {
        stats = {};
        stats.huntStarted = std::chrono::steady_clock::now();
        lastState.clear();
        actionIndex = 0;
        lastActionTime = std::chrono::steady_clock::now();
        waitingForShinyCheck = false;
        consecutiveStuckCount = 0;
    }

    std::string SoftResetStrategy::Describe() const
    {
        return "SoftResetStrategy(" + config.huntId + ", target=" + config.targetPokemon + ")";
    }

    Input::InputCommand SoftResetStrategy::BuildInputCommand(const std::vector<std::string> &buttons) const
    {
        Input::InputCommand cmd;
        for (const auto &btn : buttons)
        {
            cmd.buttonsPressed |= ButtonNameToBit(btn);
        }
        return cmd;
    }

    uint32_t SoftResetStrategy::ButtonNameToBit(const std::string &name) const
    {
        if (name == "A")
            return static_cast<uint32_t>(Input::Button::A);
        if (name == "B")
            return static_cast<uint32_t>(Input::Button::B);
        if (name == "SELECT")
            return static_cast<uint32_t>(Input::Button::Select);
        if (name == "START")
            return static_cast<uint32_t>(Input::Button::Start);
        if (name == "D_RIGHT")
            return static_cast<uint32_t>(Input::Button::DRight);
        if (name == "D_LEFT")
            return static_cast<uint32_t>(Input::Button::DLeft);
        if (name == "D_UP")
            return static_cast<uint32_t>(Input::Button::DUp);
        if (name == "D_DOWN")
            return static_cast<uint32_t>(Input::Button::DDown);
        if (name == "R")
            return static_cast<uint32_t>(Input::Button::R);
        if (name == "L")
            return static_cast<uint32_t>(Input::Button::L);
        if (name == "X")
            return static_cast<uint32_t>(Input::Button::X);
        if (name == "Y")
            return static_cast<uint32_t>(Input::Button::Y);

        LOG_WARN("Unknown button name: {}", name);
        return 0;
    }
} // namespace SH3DS::Strategy
