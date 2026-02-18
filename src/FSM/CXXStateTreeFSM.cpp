#include "CXXStateTreeFSM.h"

#include "Kappa/Logger.h"
#include "Vision/TemplateMatcher.h"

#include <CXXStateTree/StateTree.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace SH3DS::FSM
{
    // ── Builder ─────────────────────────────────────────────────────────────

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

    CXXStateTreeFSM::Builder &CXXStateTreeFSM::Builder::AddState(StateConfig config)
    {
        states.push_back(std::move(config));
        return *this;
    }

    std::unique_ptr<CXXStateTreeFSM> CXXStateTreeFSM::Builder::Build()
    {
        // Build the CXXStateTree graph for transition validation.
        // Each state gets a "goto_<target>" event for each allowed transition.
        CXXStateTree::StateTree::Builder treeBuilder;
        treeBuilder.initial(initialState);

        for (const auto &sc : states)
        {
            treeBuilder.state(sc.id, [&sc](CXXStateTree::State &s) {
                for (const auto &target : sc.transitionsTo)
                {
                    s.on("goto_" + target, target);
                }
            });
        }

        auto stateTree = std::make_unique<CXXStateTree::StateTree>(treeBuilder.build());

        return std::unique_ptr<CXXStateTreeFSM>(
            new CXXStateTreeFSM(std::move(stateTree), initialState, debounceFrames, std::move(states)));
    }

    // ── Constructor / Destructor ────────────────────────────────────────────

    CXXStateTreeFSM::CXXStateTreeFSM(std::unique_ptr<CXXStateTree::StateTree> tree,
        std::string initialState,
        int debounceFrames,
        std::vector<StateConfig> stateConfigs)
        : tree(std::move(tree))
        , initialState(std::move(initialState))
        , debounceFrames(debounceFrames)
        , stateConfigs(std::move(stateConfigs))
    {
        Reset();
    }

    CXXStateTreeFSM::~CXXStateTreeFSM() = default;

    // ── GameStateFSM Interface ──────────────────────────────────────────────

    std::optional<Core::StateTransition> CXXStateTreeFSM::Update(const Core::ROISet &rois)
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

            if (pendingFrameCount >= debounceFrames && pendingState != currentState)
            {
                // Validate transition via CXXStateTree graph
                bool allowed = true;
                const auto *config = FindStateConfig(currentState);

                if (config && !config->transitionsTo.empty())
                {
                    auto &allowedTargets = config->transitionsTo;
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

                    // Sync CXXStateTree graph state
                    try
                    {
                        tree->send("goto_" + pendingState);
                    }
                    catch (const std::runtime_error &e)
                    {
                        LOG_WARN("FSM: CXXStateTree rejected transition {} -> {}: {}",
                            currentState,
                            pendingState,
                            e.what());
                    }

                    currentState = pendingState;
                    stateEnteredAt = transition.timestamp;
                    pendingFrameCount = 0;

                    RecordTransition(transition);

                    return transition;
                }
                else
                {
                    pendingState.clear();
                    pendingFrameCount = 0;
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

    Core::GameState CXXStateTreeFSM::CurrentState() const
    {
        return currentState;
    }

    std::chrono::milliseconds CXXStateTreeFSM::TimeInCurrentState() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - stateEnteredAt);
    }

    bool CXXStateTreeFSM::IsStuck() const
    {
        const auto *config = FindStateConfig(currentState);
        if (config)
        {
            auto maxDuration = std::chrono::seconds(config->maxDurationS);
            return TimeInCurrentState() > maxDuration;
        }
        // Unknown state — use a default timeout
        return TimeInCurrentState() > std::chrono::seconds(120);
    }

    void CXXStateTreeFSM::ForceState(const Core::GameState &state)
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

    void CXXStateTreeFSM::Reset()
    {
        // Rebuild the CXXStateTree so its internal state is back at initialState,
        // not wherever ForceState or a failed send() left it.
        CXXStateTree::StateTree::Builder treeBuilder;
        treeBuilder.initial(initialState);
        for (const auto &sc : stateConfigs)
        {
            treeBuilder.state(sc.id, [&sc](CXXStateTree::State &s) {
                for (const auto &target : sc.transitionsTo)
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
        history.clear();
    }

    const std::vector<Core::StateTransition> &CXXStateTreeFSM::History() const
    {
        return history;
    }

    // ── Detection Evaluation ────────────────────────────────────────────────

    CXXStateTreeFSM::DetectionResult CXXStateTreeFSM::EvaluateRules(const Core::ROISet &rois) const
    {
        LOG_DEBUG("FSM: EvaluateRules called with {} ROIs, {} states", rois.size(), stateConfigs.size());
        DetectionResult bestResult;

        // Build candidate set: currentState + its allowed transitions.
        // allowAllTransitions=true means any state can be detected (used for unknown/recovery states).
        // Empty transitionsTo with allowAllTransitions=false means no outgoing transitions allowed.
        std::vector<std::string> candidates;

        const auto *currentConfig = FindStateConfig(currentState);
        bool constrained = currentConfig && !currentConfig->allowAllTransitions;
        if (constrained)
        {
            candidates.push_back(currentState);
            candidates.insert(
                candidates.end(), currentConfig->transitionsTo.begin(), currentConfig->transitionsTo.end());
        }

        for (const auto &sc : stateConfigs)
        {
            if (constrained)
            {
                if (std::find(candidates.begin(), candidates.end(), sc.id) == candidates.end())
                {
                    continue;
                }
            }

            const auto &params = sc.detection;
            auto it = rois.find(params.roi);
            if (it == rois.end() || it->second.empty())
            {
                continue;
            }

            const cv::Mat &roiMat = it->second;
            double confidence = 0.0;

            if (params.method == "template_match")
            {
                confidence = EvaluateTemplateMatch(roiMat, params);
            }
            else if (params.method == "color_histogram" || params.method == "pixel_ratio")
            {
                confidence = EvaluateColorHistogram(roiMat, params);
            }

            LOG_DEBUG("FSM: Evaluating Rule for state '{}' on ROI '{}': confidence={:.3f} (threshold={:.2f})",
                sc.id,
                params.roi,
                confidence,
                params.threshold);

            if (confidence >= params.threshold && confidence > bestResult.confidence)
            {
                bestResult.state = sc.id;
                bestResult.confidence = confidence;
            }
        }

        return bestResult;
    }

    double CXXStateTreeFSM::EvaluateTemplateMatch(const cv::Mat &roi, const Core::StateDetectionParams &params) const
    {
        if (params.templatePath.empty())
        {
            return 0.0;
        }

        return templateMatcher.Match(roi, params.templatePath);
    }

    double CXXStateTreeFSM::EvaluateColorHistogram(const cv::Mat &roi, const Core::StateDetectionParams &params) const
    {
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);

        cv::Mat mask;
        cv::inRange(hsv, params.hsvLower, params.hsvUpper, mask);

        double pixelRatio = cv::countNonZero(mask) / static_cast<double>(roi.total());

        if (pixelRatio >= params.pixelRatioMin && pixelRatio <= params.pixelRatioMax)
        {
            // Confidence is 1.0 at midpoint, 0.5 at boundaries of [min, max] range
            double midpoint = (params.pixelRatioMin + params.pixelRatioMax) / 2.0;
            double halfRange = (params.pixelRatioMax - params.pixelRatioMin) / 2.0;
            double distance = std::abs(pixelRatio - midpoint);
            return halfRange > 0.0 ? 1.0 - 0.5 * (distance / halfRange) : 1.0;
        }

        return 0.0;
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    void CXXStateTreeFSM::RecordTransition(const Core::StateTransition &transition)
    {
        history.push_back(transition);
        if (history.size() > 1000)
        {
            history.erase(history.begin(), history.begin() + 500);
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
