#pragma once

#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/GameStateFSM.h"
#include "Vision/TemplateMatcher.h"

#include <opencv2/core.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <CXXStateTree/StateTree.h>

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
            std::string id;                         ///< State identifier
            std::vector<std::string> transitionsTo; ///< Allowed target states (empty = no transitions)
            int maxDurationS = 60;                  ///< Watchdog timeout
            bool shinyCheck = false;                ///< Whether this state triggers shiny detection
            Core::StateDetectionParams detectionParameters; ///< Detection parameters (from YAML)
        };

        /**
         * @brief Builder for constructing a CXXStateTreeFSM.
         */
        class Builder
        {
        public:
            /**
             * @brief Sets the initial state.
             * @param state The initial state.
             * @return Builder& The builder instance.
             */
            Builder &SetInitialState(const std::string &state);

            /**
             * @brief Sets the debounce frame count.
             * @param frames The debounce frame count.
             * @return Builder& The builder instance.
             */
            Builder &SetDebounceFrames(int frames);

            /**
             * @brief Adds a state configuration.
             * @param config The state configuration.
             * @return Builder& The builder instance.
             */
            Builder &AddState(StateConfig config);

            /**
             * @brief Builds and returns the FSM.
             * @return std::unique_ptr<CXXStateTreeFSM> The built FSM.
             */
            std::unique_ptr<CXXStateTreeFSM> Build();

        private:
            std::string initialState;
            int debounceFrames = 3;
            std::vector<StateConfig> stateConfigs;
        };

        ~CXXStateTreeFSM() override = default;

        std::optional<Core::StateTransition> Update(const Core::ROISet &rois) override;

        void Reset() override;

        bool IsStuck() const override;

        const Core::GameState &GetCurrentState() const override;

        const Core::GameState &GetInitialState() const override;

        std::chrono::milliseconds GetTimeInCurrentState() const override;

        const std::vector<Core::StateTransition> &GetTransitionHistory() const override;

    private:
        /**
         * @brief Constructs a new CXXStateTreeFSM.
         * @param tree The CXXStateTree instance.
         * @param initialState The initial state ID.
         * @param debounceFrames The debounce frame count.
         * @param stateConfigs The state configurations.
         */
        explicit CXXStateTreeFSM(std::unique_ptr<CXXStateTree::StateTree> tree,
            std::string initialState,
            int debounceFrames,
            std::vector<StateConfig> stateConfigs);

        /**
         * @brief Represents the result of a detection.
         */
        struct DetectionResult
        {
            Core::GameState state;   ///< The detected state.
            double confidence = 0.0; ///< The confidence level of the detection.
        };

        /**
         * @brief Detects the best candidate state from the current ROISet.
         * @param rois The current ROISet.
         * @return DetectionResult The result of the detection.
         */
        DetectionResult DetectBestCandidateState(const Core::ROISet &rois) const;

        /**
         * @brief Evaluates the template match for a given ROI.
         * @param roi The ROI to evaluate.
         * @param stateDetectionParameters The detection parameters.
         * @return double The template match score.
         */
        double EvaluateTemplateMatch(const cv::Mat &roi,
            const Core::StateDetectionParams &stateDetectionParameters) const;

        /**
         * @brief Evaluates the color histogram for a given ROI.
         * @param roi The ROI to evaluate.
         * @param stateDetectionParameters The detection parameters.
         * @return double The color histogram score.
         */
        double EvaluateColorHistogram(const cv::Mat &roi,
            const Core::StateDetectionParams &stateDetectionParameters) const;

        /**
         * @brief Records a state transition.
         * @param transition The state transition to record.
         */
        void RecordTransition(const Core::StateTransition &transition);

        /**
         * @brief Finds a state configuration by ID.
         * @param id The state ID.
         * @return const StateConfig* A pointer to the state configuration if found, nullptr otherwise.
         */
        const StateConfig *FindStateConfig(const std::string &id) const;

        std::unique_ptr<CXXStateTree::StateTree> tree; ///< CXXStateTree instance
        std::string initialState;                      ///< Initial state ID
        int debounceFrames;                            ///< Debounce frame count
        std::vector<StateConfig> stateConfigs;         ///< All state configurations

        Core::GameState currentState;                         ///< The current state
        std::chrono::steady_clock::time_point stateEnteredAt; ///< When current state was entered
        Core::GameState pendingState;                         ///< The pending state
        int pendingFrameCount = 0;                            ///< Debounce frame counter
        std::vector<Core::StateTransition> transitionHistory; ///< Transition history
        mutable Vision::TemplateMatcher templateMatcher;      ///< Template matcher for detection
    };
} // namespace SH3DS::FSM
