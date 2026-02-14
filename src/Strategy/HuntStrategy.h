#pragma once

#include "Core/Types.h"
#include "Input/InputCommand.h"

#include <chrono>
#include <optional>
#include <string>

namespace SH3DS::Strategy
{
    /**
     * @brief Bundles a hunt decision with the input command to send.
     *
     * Keeps Core::HuntDecision free of Input dependencies while giving
     * the pipeline everything it needs to act on a strategy tick.
     */
    struct StrategyDecision
    {
        Core::HuntDecision decision; ///< What action to take and why
        Input::InputCommand command; ///< Command to send (meaningful only when action == SendInput)
    };

    /**
     * @brief Abstract base class for hunt orchestration strategies.
     */
    class HuntStrategy
    {
    public:
        virtual ~HuntStrategy() = default;

        /**
         * @brief Evaluate the current game state and decide what to do next.
         * @param currentState Current FSM state identifier.
         * @param timeInState How long the FSM has been in this state.
         * @param shinyResult Shiny detection result, if available.
         * @return Strategy decision with action and optional input command.
         */
        virtual StrategyDecision Tick(const Core::GameState &currentState,
            std::chrono::milliseconds timeInState,
            const std::optional<Core::ShinyResult> &shinyResult) = 0;

        /**
         * @brief Called when the FSM watchdog detects a stuck state.
         * @return Recovery decision with action and optional input command.
         */
        virtual StrategyDecision OnStuck() = 0;

        /**
         * @brief Returns accumulated hunt statistics.
         * @return Hunt statistics reference.
         */
        virtual const Core::HuntStatistics &Stats() const = 0;

        /**
         * @brief Resets the strategy to its initial state.
         */
        virtual void Reset() = 0;

        /**
         * @brief Returns a human-readable description of the strategy.
         * @return Description string.
         */
        virtual std::string Describe() const = 0;
    };
} // namespace SH3DS::Strategy
