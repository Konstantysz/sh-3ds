#pragma once

#include "Core/Config.h"
#include "Strategy/HuntStrategy.h"

#include <chrono>
#include <string>
#include <vector>

namespace SH3DS::Strategy
{
    /**
     * @brief Soft-reset hunt strategy. Drives the L+R+START reset cycle.
     */
    class SoftResetStrategy : public HuntStrategy
    {
    public:
        /**
         * @brief Constructs a soft-reset strategy from hunt configuration.
         * @param config Hunt configuration with input sequences per state.
         */
        explicit SoftResetStrategy(Core::HuntConfig config);

        /**
         * @brief Evaluate the current game state and decide what to do next.
         */
        StrategyDecision Tick(const Core::GameState &currentState,
            std::chrono::milliseconds timeInState,
            const std::optional<Core::ShinyResult> &shinyResult) override;

        /**
         * @brief Called when the FSM watchdog detects a stuck state.
         */
        StrategyDecision OnStuck() override;

        /**
         * @brief Returns accumulated hunt statistics.
         */
        const Core::HuntStatistics &Stats() const override;

        /**
         * @brief Resets the strategy to its initial state.
         */
        void Reset() override;

        /**
         * @brief Returns a human-readable description of the strategy.
         */
        std::string Describe() const override;

    private:
        /**
         * @brief Builds an InputCommand from a list of button names.
         */
        Input::InputCommand BuildInputCommand(const std::vector<std::string> &buttons) const;

        /**
         * @brief Maps a button name string to its bitmask value.
         */
        uint32_t ButtonNameToBit(const std::string &name) const;

        Core::HuntConfig config;                              ///< Hunt configuration
        Core::HuntStatistics stats;                           ///< Accumulated statistics
        Core::GameState lastState;                            ///< Last observed game state
        int actionIndex = 0;                                  ///< Current action index within state
        std::chrono::steady_clock::time_point lastActionTime; ///< Timestamp of last action
        bool waitingForShinyCheck = false;                    ///< Whether waiting for shiny check
        int consecutiveStuckCount = 0;                        ///< Consecutive stuck recovery count
        int shinyCheckAttempts = 0;                           ///< Consecutive frames requesting shiny check
    };
} // namespace SH3DS::Strategy
