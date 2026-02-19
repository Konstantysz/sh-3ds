#include "Input/InputCommand.h"
#include "Strategy/SoftResetStrategy.h"

#include <gtest/gtest.h>

namespace
{
    SH3DS::Core::HuntConfig MakeConfig(const std::string &shinyCheckState = "check_state", int shinyCheckFrames = 5)
    {
        SH3DS::Core::HuntConfig config;
        config.huntId = "test_hunt";
        config.shinyCheckState = shinyCheckState;
        config.shinyCheckFrames = shinyCheckFrames;
        config.shinyCheckDelayMs = 0;
        return config;
    }

    SH3DS::Core::HuntConfig MakeConfigWithAction(const std::string &state, const std::vector<std::string> &buttons)
    {
        SH3DS::Core::HuntConfig config;
        config.huntId = "test_hunt";
        config.shinyCheckState = "";
        SH3DS::Core::InputAction action;
        action.buttons = buttons;
        action.holdMs = 0;
        action.waitAfterMs = 0;
        config.actions[state] = { action };
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

TEST(SoftResetStrategy, KnownButtonNameProducesCorrectBits)
{
    // D_RIGHT = 0x0010 (per InputCommand.h). A config using "D_RIGHT" must produce non-zero bits.
    auto config = MakeConfigWithAction("nav_state", { "D_RIGHT" });
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto decision = strategy.Tick("nav_state", std::chrono::milliseconds(0), std::nullopt);
    ASSERT_EQ(decision.decision.action, SH3DS::Core::HuntAction::SendInput);
    EXPECT_EQ(decision.command.buttonsPressed, static_cast<uint32_t>(SH3DS::Input::Button::DRight));
}

TEST(SoftResetStrategy, UnknownButtonNameProducesZeroBits)
{
    // "DPAD_RIGHT" is the old wrong alias — should produce 0 bits (LOG_WARN emitted).
    auto config = MakeConfigWithAction("nav_state", { "DPAD_RIGHT" });
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto decision = strategy.Tick("nav_state", std::chrono::milliseconds(0), std::nullopt);
    ASSERT_EQ(decision.decision.action, SH3DS::Core::HuntAction::SendInput);
    EXPECT_EQ(decision.command.buttonsPressed, 0u);
}

TEST(SoftResetStrategy, MultipleKnownButtonsCombineCorrectly)
{
    // L + R + START used for soft reset = 0x0200 | 0x0100 | 0x0008 = 0x0308
    auto config = MakeConfigWithAction("soft_reset", { "L", "R", "START" });
    SH3DS::Strategy::SoftResetStrategy strategy(config);

    auto decision = strategy.Tick("soft_reset", std::chrono::milliseconds(0), std::nullopt);
    ASSERT_EQ(decision.decision.action, SH3DS::Core::HuntAction::SendInput);
    EXPECT_EQ(decision.command.buttonsPressed,
        static_cast<uint32_t>(SH3DS::Input::Button::L) | static_cast<uint32_t>(SH3DS::Input::Button::R)
            | static_cast<uint32_t>(SH3DS::Input::Button::Start));
}
