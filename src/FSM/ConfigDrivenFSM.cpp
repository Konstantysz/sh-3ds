#include "ConfigDrivenFSM.h"

#include "Kappa/Logger.h"
#include "Vision/TemplateMatcher.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace SH3DS::FSM
{
    ConfigDrivenFSM::ConfigDrivenFSM(Core::GameProfile profile) : profile(std::move(profile))
    {
        Reset();
    }

    std::optional<Core::StateTransition> ConfigDrivenFSM::Update(const Core::ROISet &rois)
    {
        auto result = EvaluateRules(rois);

        if (result.state.empty() || result.confidence < 0.01)
        {
            pendingFrameCount = 0;
            return std::nullopt;
        }

        // Debounce: require N consecutive frames detecting the same new state
        if (result.state != currentState)
        {
            if (result.state == pendingState)
            {
                ++pendingFrameCount;
            }
            else
            {
                pendingState = result.state;
                pendingFrameCount = 1;
            }

            if (pendingFrameCount >= profile.debounceFrames && pendingState != currentState)
            {
                bool allowed = true;
                auto it = std::find_if(profile.states.begin(),
                    profile.states.end(),
                    [this](const Core::StateDefinition &s) { return s.id == currentState; });

                if (it != profile.states.end() && !it->transitionsTo.empty())
                {
                    auto &allowedTargets = it->transitionsTo;
                    if (std::find(allowedTargets.begin(), allowedTargets.end(), pendingState) == allowedTargets.end())
                    {
                        LOG_WARN("FSM: Illegal transition {} -> {}! (ignoring)", currentState, pendingState);
                        allowed = false;
                    }
                }

                if (allowed)
                {
                    Core::StateTransition transition{
                        .from = currentState, .to = pendingState, .timestamp = std::chrono::steady_clock::now()
                    };

                    currentState = pendingState;
                    stateEnteredAt = transition.timestamp;
                    pendingFrameCount = 0;

                    RecordTransition(transition);

                    return transition;
                }
            }
        }
        else
        {
            pendingState.clear();
            pendingFrameCount = 0;
        }

        return std::nullopt;
    }

    Core::GameState ConfigDrivenFSM::CurrentState() const
    {
        return currentState;
    }

    std::chrono::milliseconds ConfigDrivenFSM::TimeInCurrentState() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - stateEnteredAt);
    }

    bool ConfigDrivenFSM::IsStuck() const
    {
        for (const auto &stateDef : profile.states)
        {
            if (stateDef.id == currentState)
            {
                auto maxDuration = std::chrono::seconds(stateDef.maxDurationS);
                return TimeInCurrentState() > maxDuration;
            }
        }
        // Unknown state — use a default timeout
        return TimeInCurrentState() > std::chrono::seconds(120);
    }

    void ConfigDrivenFSM::ForceState(const Core::GameState &state)
    {
        Core::StateTransition transition{
            .from = currentState, .to = state, .timestamp = std::chrono::steady_clock::now()
        };

        currentState = state;
        stateEnteredAt = transition.timestamp;
        pendingState = state;
        pendingFrameCount = 0;

        RecordTransition(transition);
    }

    void ConfigDrivenFSM::Reset()
    {
        currentState = profile.initialState;
        stateEnteredAt = std::chrono::steady_clock::now();
        pendingState.clear();
        pendingFrameCount = 0;
        history.clear();
    }

    const std::vector<Core::StateTransition> &ConfigDrivenFSM::History() const
    {
        return history;
    }

    ConfigDrivenFSM::DetectionResult ConfigDrivenFSM::EvaluateRules(const Core::ROISet &rois) const
    {
        LOG_INFO("FSM: EvaluateRules called with {} ROIs, profile has {} states", rois.size(), profile.states.size());
        DetectionResult bestResult;

        for (const auto &stateDef : profile.states)
        {
            const auto &rule = stateDef.detection;
            auto it = rois.find(rule.roi);
            if (it == rois.end() || it->second.empty())
            {
                continue;
            }

            const cv::Mat &roiMat = it->second;
            double confidence = 0.0;

            if (rule.method == "template_match")
            {
                confidence = EvaluateTemplateMatch(roiMat, rule);
            }
            else if (rule.method == "color_histogram" || rule.method == "pixel_ratio")
            {
                confidence = EvaluateColorHistogram(roiMat, rule);
            }

            LOG_DEBUG("FSM: Evaluating Rule for state '{}' on ROI '{}': confidence={:.3f} (threshold={:.2f})",
                stateDef.id,
                rule.roi,
                confidence,
                rule.threshold);

            if (confidence >= rule.threshold && confidence > bestResult.confidence)
            {
                bestResult.state = stateDef.id;
                bestResult.confidence = confidence;
            }
        }

        return bestResult;
    }

    double ConfigDrivenFSM::EvaluateTemplateMatch(const cv::Mat &roi, const Core::StateDetectionRule &rule) const
    {
        if (rule.templatePath.empty())
        {
            return 0.0;
        }

        return templateMatcher.Match(roi, rule.templatePath);
    }

    double ConfigDrivenFSM::EvaluateColorHistogram(const cv::Mat &roi, const Core::StateDetectionRule &rule) const
    {
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);

        cv::Mat mask;
        cv::inRange(hsv, rule.hsvLower, rule.hsvUpper, mask);

        double pixelRatio = cv::countNonZero(mask) / static_cast<double>(roi.total());

        if (pixelRatio >= rule.pixelRatioMin && pixelRatio <= rule.pixelRatioMax)
        {
            // Confidence is 1.0 at midpoint, 0.5 at boundaries of [min, max] range
            double midpoint = (rule.pixelRatioMin + rule.pixelRatioMax) / 2.0;
            double halfRange = (rule.pixelRatioMax - rule.pixelRatioMin) / 2.0;
            double distance = std::abs(pixelRatio - midpoint);
            return halfRange > 0.0 ? 1.0 - 0.5 * (distance / halfRange) : 1.0;
        }

        return 0.0;
    }

    void ConfigDrivenFSM::RecordTransition(const Core::StateTransition &transition)
    {
        history.push_back(transition);
        if (history.size() > 1000)
        {
            history.erase(history.begin(), history.begin() + 500);
        }
    }
} // namespace SH3DS::FSM
