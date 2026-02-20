#include "CXXStateTreeFSM.h"

#include "Kappa/Logger.h"
#include "Vision/TemplateMatcher.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace SH3DS::FSM
{
    CXXStateTreeFSM::Builder &CXXStateTreeFSM::Builder::SetInitialState(const std::string &state)
    {
        initialState = state;
        return *this;
    }

    CXXStateTreeFSM::Builder &CXXStateTreeFSM::Builder::SetDebounceFrames(int frames)
    {
        debounceFrames = frames;
        return *this;
    }

    CXXStateTreeFSM::Builder &CXXStateTreeFSM::Builder::SetScreenMode(Core::ScreenMode mode)
    {
        screenMode = mode;
        return *this;
    }

    CXXStateTreeFSM::Builder &CXXStateTreeFSM::Builder::AddState(StateConfig config)
    {
        stateConfigs.push_back(std::move(config));
        return *this;
    }

    std::unique_ptr<CXXStateTreeFSM> CXXStateTreeFSM::Builder::Build()
    {
        // Build the CXXStateTree graph for transition validation.
        // Each state gets a "goto_<target>" event for each allowed transition.
        CXXStateTree::StateTree::Builder treeBuilder;
        treeBuilder.initial(initialState);

        for (const auto &stateConfig : stateConfigs)
        {
            treeBuilder.state(stateConfig.id, [&stateConfig](CXXStateTree::State &s) {
                for (const auto &target : stateConfig.transitionsTo)
                {
                    s.on("goto_" + target, target);
                }
            });
        }

        auto stateTree = std::make_unique<CXXStateTree::StateTree>(treeBuilder.build());

        return std::unique_ptr<CXXStateTreeFSM>(new CXXStateTreeFSM(
            std::move(stateTree), initialState, debounceFrames, screenMode, std::move(stateConfigs)));
    }

    CXXStateTreeFSM::CXXStateTreeFSM(std::unique_ptr<CXXStateTree::StateTree> tree,
        std::string initialState,
        int debounceFrames,
        Core::ScreenMode screenMode,
        std::vector<StateConfig> stateConfigs)
        : tree(std::move(tree)),
          initialState(std::move(initialState)),
          debounceFrames(debounceFrames),
          screenMode(screenMode),
          stateConfigs(std::move(stateConfigs))
    {
        Reset();
    }

    std::optional<Core::StateTransition> CXXStateTreeFSM::Update(const Core::ROISet &topRois,
        const SH3DS::Core::ROISet &bottomRois)
    {
        const auto bestCandidateState = DetectBestCandidateState(topRois, bottomRois);
        if (bestCandidateState.state.empty() || bestCandidateState.confidence < 0.01)
        {
            pendingFrameCount = 0;
            return std::nullopt;
        }

        // Debounce: require N consecutive frames detecting the same new state
        if (bestCandidateState.state == currentState)
        {
            pendingState.clear();
            pendingFrameCount = 0;
            return std::nullopt;
        }

        if (bestCandidateState.state == pendingState)
        {
            ++pendingFrameCount;
        }
        else
        {
            pendingState = bestCandidateState.state;
            pendingFrameCount = 1;
        }

        if (pendingFrameCount < debounceFrames || pendingState == currentState)
        {
            return std::nullopt;
        }

        bool isTransitionAllowed = true;
        const auto *config = FindStateConfig(currentState);
        if (config && !config->transitionsTo.empty())
        {
            const auto &allowedTargets = config->transitionsTo;
            if (std::find(allowedTargets.begin(), allowedTargets.end(), pendingState) == allowedTargets.end())
            {
                LOG_WARN("FSM: Illegal transition {} -> {}! (ignoring)", currentState, pendingState);
                isTransitionAllowed = false;
            }
        }

        if (!isTransitionAllowed)
        {
            pendingState.clear();
            pendingFrameCount = 0;
            return std::nullopt;
        }

        Core::StateTransition transition{
            .from = currentState, .to = pendingState, .timestamp = std::chrono::steady_clock::now()
        };

        try
        {
            tree->send("goto_" + pendingState);
        }
        catch (const std::runtime_error &e)
        {
            LOG_ERROR("FSM: CXXStateTree rejected transition {} -> {}: {}", currentState, pendingState, e.what());

            pendingState.clear();
            pendingFrameCount = 0;
            return std::nullopt;
        }

        currentState = pendingState;
        stateEnteredAt = transition.timestamp;
        pendingFrameCount = 0;

        RecordTransition(transition);

        return transition;
    }

    void CXXStateTreeFSM::Reset()
    {
        CXXStateTree::StateTree::Builder treeBuilder;
        treeBuilder.initial(initialState);
        for (const auto &stateConfig : stateConfigs)
        {
            treeBuilder.state(stateConfig.id, [&stateConfig](CXXStateTree::State &s) {
                for (const auto &target : stateConfig.transitionsTo)
                {
                    s.on("goto_" + target, target);
                }
            });
        }
        tree = std::make_unique<CXXStateTree::StateTree>(treeBuilder.build());

        currentState = initialState;
        stateEnteredAt = std::chrono::steady_clock::now();
        pendingState.clear();
        pendingFrameCount = 0;
        transitionHistory.clear();
    }

    bool CXXStateTreeFSM::IsStuck() const
    {
        const auto *config = FindStateConfig(currentState);
        if (!config)
        {
            return GetTimeInCurrentState() > std::chrono::seconds(120);
        }

        return GetTimeInCurrentState() > std::chrono::seconds(config->maxDurationS);
    }

    const Core::GameState &CXXStateTreeFSM::GetCurrentState() const
    {
        return currentState;
    }

    const Core::GameState &CXXStateTreeFSM::GetInitialState() const
    {
        return initialState;
    }

    std::chrono::milliseconds CXXStateTreeFSM::GetTimeInCurrentState() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - stateEnteredAt);
    }

    const std::vector<Core::StateTransition> &CXXStateTreeFSM::GetTransitionHistory() const
    {
        return transitionHistory;
    }

    CXXStateTreeFSM::DetectionResult CXXStateTreeFSM::DetectBestCandidateState(const Core::ROISet &topRois,
        const SH3DS::Core::ROISet &bottomRois) const
    {
        LOG_DEBUG("FSM: EvaluateRules called with {} top ROIs, {} bottom ROIs, {} states",
            topRois.size(),
            bottomRois.size(),
            stateConfigs.size());

        DetectionResult bestResult;
        std::vector<std::string> candidates;
        const auto *currentConfig = FindStateConfig(currentState);
        const bool constrained = currentConfig != nullptr;
        if (constrained)
        {
            candidates.push_back(currentState);
            candidates.insert(
                candidates.end(), currentConfig->transitionsTo.begin(), currentConfig->transitionsTo.end());
        }

        for (const auto &stateConfig : stateConfigs)
        {
            if (constrained)
            {
                if (std::find(candidates.begin(), candidates.end(), stateConfig.id) == candidates.end())
                {
                    continue;
                }
            }

            const auto &stateDetectionParameters = stateConfig.detectionParameters;
            const bool hasTop = stateDetectionParameters.top.has_value();
            const bool hasBottom = stateDetectionParameters.bottom.has_value();
            if (!hasTop && !hasBottom)
            {
                continue;
            }

            auto evaluateForRoi = [&](const std::optional<Core::RoiDetectionParams> &roiDetectionParams,
                                      const Core::ROISet &roiSet,
                                      const char *screenLabel) -> std::optional<double> {
                if (!roiDetectionParams.has_value())
                {
                    return std::nullopt;
                }

                const auto &params = roiDetectionParams.value();
                const std::string method = params.method;

                auto it = roiSet.find(params.roi);
                if (it == roiSet.end() || it->second.empty())
                {
                    return std::nullopt;
                }

                const cv::Mat &roiMat = it->second;
                double confidence = 0.0;

                if (method == "template_match")
                {
                    confidence = EvaluateTemplateMatch(roiMat, params);
                }
                else if (method == "color_histogram" || method == "pixel_ratio")
                {
                    confidence = EvaluateColorHistogram(roiMat, params);
                }
                else
                {
                    return std::nullopt;
                }

                LOG_DEBUG("FSM: Evaluating Rule for state '{}' on {} ROI '{}': confidence={:.3f} (threshold={:.2f})",
                    stateConfig.id,
                    screenLabel,
                    params.roi,
                    confidence,
                    params.threshold);

                if (confidence < params.threshold)
                {
                    return std::nullopt;
                }

                return confidence;
            };

            double combinedConfidence = 0.0;
            if (screenMode == Core::ScreenMode::Single)
            {
                auto evaluateSingleScreenBlock =
                    [&](const std::optional<Core::RoiDetectionParams> &block) -> std::optional<double> {
                    auto topConfidence = evaluateForRoi(block, topRois, "top");
                    if (topConfidence.has_value())
                    {
                        return topConfidence;
                    }
                    return evaluateForRoi(block, bottomRois, "bottom");
                };

                if (hasTop)
                {
                    auto topConfidence = evaluateSingleScreenBlock(stateDetectionParameters.top);
                    if (!topConfidence.has_value())
                    {
                        continue;
                    }
                    combinedConfidence = topConfidence.value();
                }
                else
                {
                    auto bottomConfidence = evaluateSingleScreenBlock(stateDetectionParameters.bottom);
                    if (!bottomConfidence.has_value())
                    {
                        continue;
                    }
                    combinedConfidence = bottomConfidence.value();
                }
            }
            else
            {
                auto topConfidence = evaluateForRoi(stateDetectionParameters.top, topRois, "top");
                auto bottomConfidence = evaluateForRoi(stateDetectionParameters.bottom, bottomRois, "bottom");

                if (hasTop && hasBottom)
                {
                    if (!topConfidence.has_value() || !bottomConfidence.has_value())
                    {
                        continue;
                    }
                    combinedConfidence = std::min(topConfidence.value(), bottomConfidence.value());
                }
                else if (hasTop)
                {
                    if (!topConfidence.has_value())
                    {
                        continue;
                    }
                    combinedConfidence = topConfidence.value();
                }
                else
                {
                    if (!bottomConfidence.has_value())
                    {
                        continue;
                    }
                    combinedConfidence = bottomConfidence.value();
                }
            }

            if (combinedConfidence > bestResult.confidence)
            {
                bestResult.state = stateConfig.id;
                bestResult.confidence = combinedConfidence;
            }
        }

        return bestResult;
    }

    double CXXStateTreeFSM::EvaluateTemplateMatch(const cv::Mat &roi,
        const Core::RoiDetectionParams &roiDetectionParameters) const
    {
        if (roiDetectionParameters.templatePath.empty())
        {
            return 0.0;
        }

        return templateMatcher.Match(roi, roiDetectionParameters.templatePath);
    }

    double CXXStateTreeFSM::EvaluateColorHistogram(const cv::Mat &roi,
        const Core::RoiDetectionParams &roiDetectionParameters) const
    {
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);

        cv::Mat mask;
        cv::inRange(hsv, roiDetectionParameters.hsvLower, roiDetectionParameters.hsvUpper, mask);

        const double pixelRatio = cv::countNonZero(mask) / static_cast<double>(roi.total());

        if (pixelRatio < roiDetectionParameters.pixelRatioMin || pixelRatio > roiDetectionParameters.pixelRatioMax)
        {
            return 0.0;
        }

        const double midpoint = (roiDetectionParameters.pixelRatioMin + roiDetectionParameters.pixelRatioMax) / 2.0;
        const double halfRange = (roiDetectionParameters.pixelRatioMax - roiDetectionParameters.pixelRatioMin) / 2.0;
        const double distance = std::abs(pixelRatio - midpoint);

        return halfRange > 0.0 ? 1.0 - 0.5 * (distance / halfRange) : 1.0;
    }

    void CXXStateTreeFSM::RecordTransition(const Core::StateTransition &transition)
    {
        transitionHistory.push_back(transition);
        if (transitionHistory.size() > 1000)
        {
            transitionHistory.erase(transitionHistory.begin(), transitionHistory.begin() + 500);
        }
    }

    const CXXStateTreeFSM::StateConfig *CXXStateTreeFSM::FindStateConfig(const std::string &id) const
    {
        for (const auto &sc : stateConfigs)
        {
            if (sc.id == id)
            {
                return &sc;
            }
        }
        return nullptr;
    }
} // namespace SH3DS::FSM
