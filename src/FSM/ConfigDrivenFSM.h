#pragma once

#include "Core/Config.h"
#include "FSM/GameStateFSM.h"

#include <opencv2/core.hpp>

#include <map>
#include <vector>

namespace SH3DS::FSM
{
    /**
     * @brief Config-driven FSM implementation. States and detection rules loaded from YAML.
     */
    class ConfigDrivenFSM : public GameStateFSM
    {
    public:
        /**
         * @brief Constructs a new ConfigDrivenFSM.
         * @param profile The game profile to use for state detection.
         */
        explicit ConfigDrivenFSM(Core::GameProfile profile);

        /**
         * @brief Updates the FSM with the current ROISet.
         * @param rois The ROISet to update with.
         * @return An optional containing the state transition if successful, an empty optional otherwise.
         */
        std::optional<Core::StateTransition> Update(const Core::ROISet &rois) override;

        /**
         * @brief Gets the current state.
         * @return The current state.
         */
        Core::GameState CurrentState() const override;

        /**
         * @brief Gets the time in the current state.
         * @return The time in the current state.
         */
        std::chrono::milliseconds TimeInCurrentState() const override;

        /**
         * @brief Checks if the FSM is stuck.
         * @return True if the FSM is stuck, false otherwise.
         */
        bool IsStuck() const override;

        /**
         * @brief Forces the FSM to a specific state.
         * @param state The state to force the FSM to.
         */
        void ForceState(const Core::GameState &state) override;

        /**
         * @brief Resets the FSM.
         */
        void Reset() override;

        /**
         * @brief Gets the history of state transitions.
         * @return The history of state transitions.
         */
        const std::vector<Core::StateTransition> &History() const override;

    private:
        /**
         * @brief Represents the result of a state detection.
         */
        struct DetectionResult
        {
            Core::GameState state;   ///< The detected state.
            double confidence = 0.0; ///< The confidence level of the detection.
        };

        /**
         * @brief Evaluates the detection rules for the given ROISet.
         * @param rois The ROISet to evaluate.
         * @return The detection result.
         */
        DetectionResult EvaluateRules(const Core::ROISet &rois) const;

        /**
         * @brief Evaluates the template match for the given ROI and rule.
         * @param roi The ROI to evaluate.
         * @param rule The rule to use for evaluation.
         * @return The template match result.
         */
        double EvaluateTemplateMatch(const cv::Mat &roi, const Core::StateDetectionRule &rule) const;

        /**
         * @brief Evaluates the color histogram for the given ROI and rule.
         * @param roi The ROI to evaluate.
         * @param rule The rule to use for evaluation.
         * @return The color histogram result.
         */
        double EvaluateColorHistogram(const cv::Mat &roi, const Core::StateDetectionRule &rule) const;

        /**
         * @brief Records a transition in history and prunes if necessary.
         * @param transition The transition to record.
         */
        void RecordTransition(const Core::StateTransition &transition);


        Core::GameProfile profile;                            ///< The game profile.
        Core::GameState currentState;                         ///< The current state.
        std::chrono::steady_clock::time_point stateEnteredAt; ///< The time when the current state was entered.
        Core::GameState pendingState;                         ///< The pending state.
        int pendingFrameCount = 0;                            ///< The number of frames in the pending state.
        std::vector<Core::StateTransition> history;           ///< The history of state transitions.
        mutable std::map<std::string, cv::Mat> templateCache; ///< The cache for template images.
    };
} // namespace SH3DS::FSM
