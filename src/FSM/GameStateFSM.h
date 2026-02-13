#pragma once

#include "Core/Types.h"

#include <chrono>
#include <optional>
#include <vector>

namespace SH3DS::FSM
{
    /**
     * @brief Abstract base class for game state tracking via frame analysis.
     */
    class GameStateFSM
    {
    public:
        virtual ~GameStateFSM() = default;

        /**
         * @brief Updates the FSM with the current ROISet.
         * @param rois The ROISet to update with.
         * @return An optional containing the state transition if successful, an empty optional otherwise.
         */
        virtual std::optional<Core::StateTransition> Update(const Core::ROISet &rois) = 0;

        /**
         * @brief Gets the current state.
         * @return The current state.
         */
        virtual Core::GameState CurrentState() const = 0;
        /**
         * @brief Gets the time in the current state.
         * @return The time in the current state.
         */
        virtual std::chrono::milliseconds TimeInCurrentState() const = 0;

        /**
         * @brief Checks if the FSM is stuck.
         * @return True if the FSM is stuck, false otherwise.
         */
        virtual bool IsStuck() const = 0;
        /**
         * @brief Forces the FSM to a specific state.
         * @param state The state to force the FSM to.
         */
        virtual void ForceState(const Core::GameState &state) = 0;

        /**
         * @brief Resets the FSM.
         */
        virtual void Reset() = 0;

        /**
         * @brief Gets the history of state transitions.
         * @return The history of state transitions.
         */
        virtual const std::vector<Core::StateTransition> &History() const = 0;
    };

} // namespace SH3DS::FSM
