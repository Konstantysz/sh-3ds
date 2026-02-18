#pragma once

#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/GameStateFSM.h"
#include "Vision/TemplateMatcher.h"

#include <opencv2/core.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace CXXStateTree
{
    class StateTree;
} // namespace CXXStateTree

namespace SH3DS::FSM
{
    /**
     * @brief Adapter that wraps a CXXStateTree instance behind the GameStateFSM interface.
     *
     * States and transitions are defined in C++ via the Builder.
     * Detection parameters (HSV ranges, thresholds) come from YAML config.
     * The CXXStateTree graph validates transition legality.
     */
    class CXXStateTreeFSM : public GameStateFSM
    {
    public:
        /**
         * @brief State definition for the builder.
         */
        struct StateConfig
        {
            std::string id;                                  ///< State identifier
            std::vector<std::string> transitionsTo;          ///< Allowed target states (empty = no transitions)
            int maxDurationS = 60;                           ///< Watchdog timeout
            bool shinyCheck = false;                         ///< Whether this state triggers shiny detection
            bool allowAllTransitions = false;                ///< If true, all states are reachable (wildcard for unknown/recovery)
            Core::StateDetectionParams detection;            ///< Detection parameters (from YAML)
        };

        /**
         * @brief Builder for constructing a CXXStateTreeFSM.
         */
        class Builder
        {
        public:
            /** @brief Sets the initial state. */
            Builder &SetInitialState(const std::string &state);

            /** @brief Sets the debounce frame count. */
            Builder &SetDebounceFrames(int frames);

            /** @brief Adds a state configuration. */
            Builder &AddState(StateConfig config);

            /** @brief Builds and returns the FSM. */
            std::unique_ptr<CXXStateTreeFSM> Build();

        private:
            std::string initialState;
            int debounceFrames = 3;
            std::vector<StateConfig> states;
        };

        ~CXXStateTreeFSM() override;

        /** @brief Updates the FSM with the current ROISet. */
        std::optional<Core::StateTransition> Update(const Core::ROISet &rois) override;

        /** @brief Gets the current state. */
        Core::GameState CurrentState() const override;

        /** @brief Gets the time in the current state. */
        std::chrono::milliseconds TimeInCurrentState() const override;

        /** @brief Checks if the FSM is stuck. */
        bool IsStuck() const override;

        /** @brief Forces the FSM to a specific state. */
        void ForceState(const Core::GameState &state) override;

        /** @brief Resets the FSM. */
        void Reset() override;

        /** @brief Gets the history of state transitions. */
        const std::vector<Core::StateTransition> &History() const override;

    private:
        explicit CXXStateTreeFSM(std::unique_ptr<CXXStateTree::StateTree> tree,
            std::string initialState,
            int debounceFrames,
            std::vector<StateConfig> stateConfigs);

        struct DetectionResult
        {
            Core::GameState state;   ///< The detected state.
            double confidence = 0.0; ///< The confidence level of the detection.
        };

        DetectionResult EvaluateRules(const Core::ROISet &rois) const;
        double EvaluateTemplateMatch(const cv::Mat &roi, const Core::StateDetectionParams &params) const;
        double EvaluateColorHistogram(const cv::Mat &roi, const Core::StateDetectionParams &params) const;
        void RecordTransition(const Core::StateTransition &transition);

        const StateConfig *FindStateConfig(const std::string &id) const;

        std::unique_ptr<CXXStateTree::StateTree> tree;       ///< CXXStateTree instance
        std::string initialState;                              ///< Initial state ID
        int debounceFrames;                                    ///< Debounce frame count
        std::vector<StateConfig> stateConfigs;                 ///< All state configurations

        Core::GameState currentState;                          ///< The current state
        std::chrono::steady_clock::time_point stateEnteredAt;  ///< When current state was entered
        Core::GameState pendingState;                          ///< The pending state
        int pendingFrameCount = 0;                             ///< Debounce frame counter
        std::vector<Core::StateTransition> history;            ///< Transition history
        mutable Vision::TemplateMatcher templateMatcher;       ///< Template matcher for detection
    };
} // namespace SH3DS::FSM
