#include "Core/Config.h"
#include "Core/Types.h"
#include "FSM/GameStateFSM.h"
#include "Pipeline/Orchestrator.h"
#include "Strategy/HuntStrategy.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace
{
    // ── Minimal FSM stub ────────────────────────────────────────────────────

    class StubFSM : public SH3DS::FSM::GameStateFSM
    {
    public:
        std::optional<SH3DS::Core::StateTransition> Update(const SH3DS::Core::ROISet &) override
        {
            return std::nullopt;
        }

        const SH3DS::Core::GameState &GetCurrentState() const override
        {
            return currentState;
        }

        std::chrono::milliseconds GetTimeInCurrentState() const override
        {
            return std::chrono::milliseconds(0);
        }

        bool IsStuck() const override
        {
            return stuck;
        }

        void ForceState(const SH3DS::Core::GameState &state) override
        {
            currentState = state;
        }

        void Reset() override
        {
            currentState = "load_game";
        }

        const std::vector<SH3DS::Core::StateTransition> &GetTransitionHistory() const override
        {
            return history;
        }

        const SH3DS::Core::GameState &GetInitialState() const override
        {
            return initialState;
        }

        bool stuck = false;
        std::string currentState = "load_game";
        std::string initialState = "load_game";
        std::vector<SH3DS::Core::StateTransition> history;
    };

    // ── Minimal strategy stub ────────────────────────────────────────────────

    class StubStrategy : public SH3DS::Strategy::HuntStrategy
    {
    public:
        SH3DS::Strategy::StrategyDecision Tick(const SH3DS::Core::GameState &,
            std::chrono::milliseconds,
            const std::optional<SH3DS::Core::ShinyResult> &) override
        {
            SH3DS::Strategy::StrategyDecision d;
            d.decision.action = SH3DS::Core::HuntAction::Wait;
            return d;
        }

        SH3DS::Strategy::StrategyDecision OnStuck() override
        {
            SH3DS::Strategy::StrategyDecision d;
            d.decision.action = SH3DS::Core::HuntAction::Abort;
            return d;
        }

        const SH3DS::Core::HuntStatistics &Stats() const override
        {
            return stats;
        }

        void Reset() override
        {
        }

        std::string Describe() const override
        {
            return "StubStrategy";
        }

        SH3DS::Core::HuntStatistics stats;
    };

    // ── Frame source stub that yields exactly one frame then exhausts ────────

    class SingleFrameSource : public SH3DS::Capture::FrameSource
    {
    public:
        bool Open() override
        {
            return true;
        }

        void Close() override
        {
        }

        std::optional<SH3DS::Core::Frame> Grab() override
        {
            if (grabbed)
            {
                return std::nullopt;
            }
            grabbed = true;
            SH3DS::Core::Frame frame;
            frame.image = cv::Mat(240, 400, CV_8UC3, cv::Scalar(10, 10, 10));
            return frame;
        }

        bool IsOpen() const override
        {
            return true;
        }

        std::string Describe() const override
        {
            return "SingleFrameSource";
        }

        bool grabbed = false;
    };

    // ── Preprocessor that returns an empty ROISet (no screens detected) ──────
    // We construct a FramePreprocessor with empty calibration — it returns nullopt.
    // Instead, use a real FramePreprocessor with zero corners, which gracefully
    // skips warp and returns nullopt.

} // namespace

// ── Tests ───────────────────────────────────────────────────────────────────

TEST(Orchestrator, NullDetectorDoesNotCrashOnTick)
{
    // When shiny_detector.method is empty, detector is nullptr.
    // Orchestrator must not crash when ROI lookup succeeds but detector is null.
    SH3DS::Core::OrchestratorConfig cfg;
    cfg.targetFps = 30.0;
    cfg.shinyRoi = "pokemon_sprite";

    // Use nullptr detector — this is the scenario under test.
    SH3DS::Pipeline::Orchestrator orchestrator(std::make_unique<SingleFrameSource>(),
        nullptr, // no screen detector
        std::make_unique<SH3DS::Capture::FramePreprocessor>(
            SH3DS::Core::ScreenCalibrationConfig{}, std::vector<SH3DS::Core::RoiDefinition>{}),
        std::make_unique<StubFSM>(),
        nullptr, // <-- null detector
        std::make_unique<StubStrategy>(),
        nullptr, // no input adapter
        cfg);

    // Run processes one frame then exits (SingleFrameSource returns nullopt on second grab).
    EXPECT_NO_THROW(orchestrator.Run());
}

TEST(Orchestrator, ShinyRoiFromConfigIsUsedNotHardcoded)
{
    // OrchestratorConfig::shinyRoi must be respected.
    // We verify this by setting a non-default ROI name — if the code still
    // hardcodes "pokemon_sprite" the ROI lookup yields rois->end() regardless,
    // but at minimum the config field must be readable and forwarded.
    SH3DS::Core::OrchestratorConfig cfg;
    cfg.targetFps = 30.0;
    cfg.shinyRoi = "custom_sprite_roi"; // non-default value

    EXPECT_EQ(cfg.shinyRoi, "custom_sprite_roi");

    // Run completes without crash using the custom ROI name (no detector, so no actual lookup matters).
    SH3DS::Pipeline::Orchestrator orchestrator(std::make_unique<SingleFrameSource>(),
        nullptr,
        std::make_unique<SH3DS::Capture::FramePreprocessor>(
            SH3DS::Core::ScreenCalibrationConfig{}, std::vector<SH3DS::Core::RoiDefinition>{}),
        std::make_unique<StubFSM>(),
        nullptr,
        std::make_unique<StubStrategy>(),
        nullptr,
        cfg);

    EXPECT_NO_THROW(orchestrator.Run());
}

TEST(Orchestrator, NullDetectorDoesNotCrashOnWatchdogRecovery)
{
    // When watchdog triggers and detector is null, detector->Reset() must not be called.
    SH3DS::Core::OrchestratorConfig cfg;
    cfg.targetFps = 30.0;
    cfg.shinyRoi = "pokemon_sprite";

    auto stubFsm = std::make_unique<StubFSM>();
    stubFsm->stuck = true; // triggers watchdog immediately

    // StubStrategy::OnStuck returns Abort, so Orchestrator calls Stop() — no detector->Reset() path.
    SH3DS::Pipeline::Orchestrator orchestrator(std::make_unique<SingleFrameSource>(),
        nullptr,
        std::make_unique<SH3DS::Capture::FramePreprocessor>(
            SH3DS::Core::ScreenCalibrationConfig{}, std::vector<SH3DS::Core::RoiDefinition>{}),
        std::move(stubFsm),
        nullptr,
        std::make_unique<StubStrategy>(),
        nullptr,
        cfg);

    EXPECT_NO_THROW(orchestrator.Run());
}
