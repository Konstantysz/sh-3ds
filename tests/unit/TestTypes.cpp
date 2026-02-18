#include "Core/Types.h"
#include "Input/InputCommand.h"

#include <gtest/gtest.h>

// --- Frame ---
TEST(FrameMetadata, DefaultConstructionHasZeroSequence)
{
    SH3DS::Core::FrameMetadata meta;
    EXPECT_EQ(meta.sequenceNumber, 0u);
    EXPECT_EQ(meta.sourceWidth, 0);
    EXPECT_EQ(meta.sourceHeight, 0);
    EXPECT_DOUBLE_EQ(meta.fpsEstimate, 0.0);
}

TEST(Frame, ConstructWithImageAndMetadata)
{
    cv::Mat image(240, 400, CV_8UC3, cv::Scalar(0, 0, 0));
    SH3DS::Core::FrameMetadata meta{
        .sequenceNumber = 42,
        .captureTime = std::chrono::steady_clock::now(),
        .sourceWidth = 400,
        .sourceHeight = 240,
        .fpsEstimate = 12.0,
    };

    SH3DS::Core::Frame frame{ .image = image, .metadata = meta };
    EXPECT_EQ(frame.image.cols, 400);
    EXPECT_EQ(frame.image.rows, 240);
    EXPECT_EQ(frame.metadata.sequenceNumber, 42u);
    EXPECT_DOUBLE_EQ(frame.metadata.fpsEstimate, 12.0);
}

TEST(Frame, ImageShallowCopySharesData)
{
    cv::Mat image(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    SH3DS::Core::Frame frame{ .image = image, .metadata = {} };
    EXPECT_EQ(frame.image.data, image.data);
}

// --- ShinyVerdict ---

TEST(ShinyVerdict, EnumValues)
{
    EXPECT_NE(SH3DS::Core::ShinyVerdict::NotShiny, SH3DS::Core::ShinyVerdict::Shiny);
    EXPECT_NE(SH3DS::Core::ShinyVerdict::Shiny, SH3DS::Core::ShinyVerdict::Uncertain);
    EXPECT_NE(SH3DS::Core::ShinyVerdict::NotShiny, SH3DS::Core::ShinyVerdict::Uncertain);
}

TEST(ShinyResult, ConstructWithVerdict)
{
    SH3DS::Core::ShinyResult result{
        .verdict = SH3DS::Core::ShinyVerdict::Shiny,
        .confidence = 0.95,
        .method = "dominant_color",
        .details = "shiny ratio: 0.25",
        .debugImage = {},
    };
    EXPECT_EQ(result.verdict, SH3DS::Core::ShinyVerdict::Shiny);
    EXPECT_DOUBLE_EQ(result.confidence, 0.95);
    EXPECT_EQ(result.method, "dominant_color");
}

// --- GameState ---

TEST(GameState, IsStringAlias)
{
    SH3DS::Core::GameState state = "title_screen";
    EXPECT_EQ(state, "title_screen");
}

TEST(StateTransition, StoresFromAndTo)
{
    SH3DS::Core::StateTransition transition{
        .from = "title_screen",
        .to = "intro_cutscene",
        .timestamp = std::chrono::steady_clock::now(),
    };
    EXPECT_EQ(transition.from, "title_screen");
    EXPECT_EQ(transition.to, "intro_cutscene");
}

// --- InputCommand ---

TEST(Button, BitValues)
{
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::A), 0x0001u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::B), 0x0002u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::Select), 0x0004u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::Start), 0x0008u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::DRight), 0x0010u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::DLeft), 0x0020u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::DUp), 0x0040u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::DDown), 0x0080u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::R), 0x0100u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::L), 0x0200u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::X), 0x0400u);
    EXPECT_EQ(static_cast<uint32_t>(SH3DS::Input::Button::Y), 0x0800u);
}

TEST(Button, SoftResetCombo)
{
    uint32_t softReset = static_cast<uint32_t>(SH3DS::Input::Button::L) | static_cast<uint32_t>(SH3DS::Input::Button::R)
                         | static_cast<uint32_t>(SH3DS::Input::Button::Start);
    EXPECT_EQ(softReset, 0x0308u);
}

TEST(InputCommand, DefaultHasNoButtonsPressed)
{
    SH3DS::Input::InputCommand cmd;
    EXPECT_EQ(cmd.buttonsPressed, 0u);
    EXPECT_FLOAT_EQ(cmd.circlePad.x, 0.0f);
    EXPECT_FLOAT_EQ(cmd.circlePad.y, 0.0f);
    EXPECT_FLOAT_EQ(cmd.cStick.x, 0.0f);
    EXPECT_FLOAT_EQ(cmd.cStick.y, 0.0f);
    EXPECT_FALSE(cmd.touch.touching);
    EXPECT_EQ(cmd.interfaceButtons, 0u);
}

TEST(InputCommand, SetMultipleButtons)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed =
        static_cast<uint32_t>(SH3DS::Input::Button::A) | static_cast<uint32_t>(SH3DS::Input::Button::B);
    EXPECT_EQ(cmd.buttonsPressed, 0x0003u);
}

// --- HuntStatistics ---

TEST(HuntStatistics, DefaultZeroed)
{
    SH3DS::Core::HuntStatistics stats;
    EXPECT_EQ(stats.encounters, 0u);
    EXPECT_EQ(stats.shiniesFound, 0u);
    EXPECT_DOUBLE_EQ(stats.avgCycleSeconds, 0.0);
    EXPECT_EQ(stats.errors, 0u);
    EXPECT_EQ(stats.watchdogRecoveries, 0u);
}

// --- HuntDecision ---

TEST(HuntDecision, ConstructWithAction)
{
    SH3DS::Core::HuntDecision decision{
        .action = SH3DS::Core::HuntAction::SendInput,
        .reason = "press A on title screen",
        .delay = std::chrono::milliseconds(100),
    };
    EXPECT_EQ(decision.action, SH3DS::Core::HuntAction::SendInput);
    EXPECT_EQ(decision.reason, "press A on title screen");
}
