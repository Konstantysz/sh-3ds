#include "Strategy/SoftResetStrategy.h"

#include <gtest/gtest.h>

namespace
{
    SH3DS::Core::HuntConfig MakeConfig(const std::string &shinyCheckState = "check_state",
        int shinyCheckFrames = 5)
    {
        SH3DS::Core::HuntConfig config;
        config.huntId = "test_hunt";
        config.shinyCheckState = shinyCheckState;
        config.shinyCheckFrames = shinyCheckFrames;
        config.shinyCheckDelayMs = 0;
        return config;
    }

} // namespace

TEST(SoftResetStrategy, NullDetectorDoesNotLoopForever)
{
    // shinyCheckState is set, but no detector exists — orchestrator always passes nullopt.
    // Strategy should give up after shinyCheckFrames requests and stop returning CheckShiny.
    auto config = MakeConfig("check_state", 3);
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto noResult = std::optional<SH3DS::Core::ShinyResult>{};

    int checkShinyCount = 0;
    for (int i = 0; i < 20; ++i)
    {
        auto decision = strategy.Tick("check_state", std::chrono::milliseconds(9999), noResult);
        if (decision.decision.action == SH3DS::Core::HuntAction::CheckShiny)
        {
            ++checkShinyCount;
        }
    }

    // Should have requested CheckShiny at most shinyCheckFrames (3) times, then given up.
    EXPECT_LE(checkShinyCount, 3);
}

TEST(SoftResetStrategy, EmptyShinyCheckStateSkipsCheck)
{
    // If shinyCheckState is empty, strategy should never return CheckShiny.
    auto config = MakeConfig("");
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto noResult = std::optional<SH3DS::Core::ShinyResult>{};

    for (int i = 0; i < 5; ++i)
    {
        auto decision = strategy.Tick("any_state", std::chrono::milliseconds(0), noResult);
        EXPECT_NE(decision.decision.action, SH3DS::Core::HuntAction::CheckShiny);
    }
}

TEST(SoftResetStrategy, ShinyResultTriggersAlert)
{
    auto config = MakeConfig("check_state");
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    SH3DS::Core::ShinyResult shiny{
        .verdict = SH3DS::Core::ShinyVerdict::Shiny,
        .confidence = 0.9,
        .method = "dominant_color",
        .details = "shiny detected",
        .debugImage = {},
    };

    auto decision = strategy.Tick("check_state", std::chrono::milliseconds(9999), shiny);
    EXPECT_EQ(decision.decision.action, SH3DS::Core::HuntAction::AlertShiny);
}

TEST(SoftResetStrategy, NotShinyResultContinues)
{
    auto config = MakeConfig("check_state");
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    SH3DS::Core::ShinyResult notShiny{
        .verdict = SH3DS::Core::ShinyVerdict::NotShiny,
        .confidence = 0.95,
        .method = "dominant_color",
        .details = "not shiny",
        .debugImage = {},
    };

    auto decision = strategy.Tick("check_state", std::chrono::milliseconds(9999), notShiny);
    // After NotShiny verdict, strategy falls through to action lookup (no actions -> Wait)
    EXPECT_NE(decision.decision.action, SH3DS::Core::HuntAction::CheckShiny);
    EXPECT_NE(decision.decision.action, SH3DS::Core::HuntAction::AlertShiny);
}

TEST(SoftResetStrategy, ResetClearsShinyCheckAttempts)
{
    auto config = MakeConfig("check_state", 2);
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto noResult = std::optional<SH3DS::Core::ShinyResult>{};

    // Exhaust the attempts
    for (int i = 0; i < 5; ++i)
    {
        strategy.Tick("check_state", std::chrono::milliseconds(9999), noResult);
    }

    // After reset, should be able to request CheckShiny again
    strategy.Reset();
    auto decision = strategy.Tick("check_state", std::chrono::milliseconds(9999), noResult);
    EXPECT_EQ(decision.decision.action, SH3DS::Core::HuntAction::CheckShiny);
}
