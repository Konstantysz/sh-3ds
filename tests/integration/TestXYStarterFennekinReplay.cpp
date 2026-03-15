#include "Capture/FileFrameSource.h"
#include "Capture/FramePreprocessor.h"
#include "Capture/ScreenDetector.h"
#include "Core/Config.h"
#include "FSM/HuntProfiles.h"
#include "Vision/ColorImprovement.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /// Shared pipeline runner — same logic as Orchestrator::MainLoopTick.
    /// Returns a per-frame state log (0-based indices).
    std::vector<std::string> RunPipeline(const std::filesystem::path &imagesPath,
        const std::filesystem::path &huntCfgPath)
    {
        auto unified = SH3DS::Core::LoadUnifiedHuntConfig(huntCfgPath.string());

        auto frameSource = SH3DS::Capture::FileFrameSource::CreateFileFrameSource(imagesPath, 0.0);
        if (!frameSource || !frameSource->Open())
        {
            return {};
        }

        auto screenDetector = SH3DS::Capture::ScreenDetector::CreateScreenDetector();
        SH3DS::Core::ScreenCalibrationConfig topCalib;
        SH3DS::Capture::FramePreprocessor preprocessor(topCalib, unified.rois, std::nullopt);
        auto fsm = SH3DS::FSM::HuntProfiles::CreateXYStarterSR(unified.fsmParams);

        std::vector<std::string> stateLog;
        std::string lastState = fsm->GetCurrentState();

        while (true)
        {
            auto frame = frameSource->Grab();
            if (!frame.has_value())
            {
                break;
            }

            screenDetector->ApplyTo(preprocessor, frame->image);

            auto dualResult = preprocessor.ProcessDualScreen(frame->image);
            if (!dualResult.has_value())
            {
                stateLog.push_back(lastState);
                continue;
            }

            if (!dualResult->warpedTop.empty())
            {
                dualResult->warpedTop = SH3DS::Vision::ImproveFrameColors(dualResult->warpedTop);
                preprocessor.ReextractRois(*dualResult);
            }

            fsm->Update(dualResult->topRois, dualResult->bottomRois);
            lastState = fsm->GetCurrentState();
            stateLog.push_back(lastState);
        }

        frameSource->Close();
        return stateLog;
    }
} // namespace

/// Integration test: runs the real pipeline against the Fennekin video replay
/// and asserts the FSM reaches expected states at safe midpoint frames.
///
/// All config comes from YAML — no hardcoded detection params.
/// Pipeline mirrors Orchestrator::MainLoopTick exactly.
TEST(XYStarterFennekinReplay, StateSequenceMatchesExpected)
{
    const std::filesystem::path kRepo = SH3DS_REPO_ROOT;
    const auto imagesPath = kRepo / "video_replays/fennekin_normal/images";
    const auto huntCfgPath = kRepo / "config/hunts/xy_starter_sr_fennekin.yaml";

    if (!std::filesystem::exists(imagesPath) || std::filesystem::is_empty(imagesPath))
    {
        GTEST_SKIP() << "Replay frames not found at " << imagesPath << " — skipping video replay test.";
    }

    auto stateLog = RunPipeline(imagesPath, huntCfgPath);
    ASSERT_GE(stateLog.size(), 590u) << "Expected at least 590 frames; got " << stateLog.size();

    // Safe midpoint assertions — well inside each segment to avoid boundary sensitivity.
    // Frame indices are 0-based. Actual transition boundaries on this replay:
    //
    // | Segment          | User-given frames | Sample frame |
    // |------------------|-------------------|--------------|
    // | load_game        | 0–30              | 15           |
    // | game_start       | 31–54             | 42           |
    // | cutscene_part_1  | 55–192            | 120          |
    // | starter_pick     | 193–247           | 220          |
    // | cutscene_part_2  | 248–455           | 350          |
    // | game_menu        | 456–471           | 464          |
    // | party_menu       | 472–490           | 482          |
    // | pokemon_summary  | 491–511           | 502          |
    // | resetting        | 512–599           | 555          |
    // | load_game (loop) | 600–604           | 602          |
    //
    EXPECT_EQ(stateLog[15], "load_game") << "Frame 15: expected load_game";
    EXPECT_EQ(stateLog[42], "game_start") << "Frame 42: expected game_start";
    EXPECT_EQ(stateLog[120], "cutscene_part_1") << "Frame 120: expected cutscene_part_1";
    EXPECT_EQ(stateLog[220], "starter_pick") << "Frame 220: expected starter_pick";
    EXPECT_EQ(stateLog[350], "cutscene_part_2") << "Frame 350: expected cutscene_part_2";
    EXPECT_EQ(stateLog[464], "game_menu") << "Frame 464: expected game_menu";
    EXPECT_EQ(stateLog[481], "party_menu") << "Frame 481: expected party_menu";
    EXPECT_EQ(stateLog[508], "pokemon_summary") << "Frame 508: expected pokemon_summary";
    EXPECT_EQ(stateLog[555], "resetting") << "Frame 555: expected resetting";
    EXPECT_EQ(stateLog[602], "load_game") << "Frame 602: expected load_game (loop)";
}

/// Integration test: runs the real pipeline against live camera footage (debug_session.mp4
/// extracted to video_replays/fennekin_live/images/).
///
/// This dataset exercises the camera-specific challenges absent in the video replay:
/// MJPEG compression artifacts, JPEG blocking, auto-exposure drift, and physical
/// button presses visible in frame. Use this test to catch regressions on real data.
///
/// Frame boundaries were identified manually. Update the table below whenever the
/// live recording is replaced.
TEST(XYStarterFennekinLive, StateSequenceMatchesExpected)
{
    const std::filesystem::path kRepo = SH3DS_REPO_ROOT;
    const auto imagesPath = kRepo / "video_replays/fennekin_live/images";
    const auto huntCfgPath = kRepo / "config/hunts/xy_starter_sr_fennekin.yaml";

    if (!std::filesystem::exists(imagesPath) || std::filesystem::is_empty(imagesPath))
    {
        GTEST_SKIP() << "Live frames not found at " << imagesPath << " — skipping live camera test.";
    }

    auto stateLog = RunPipeline(imagesPath, huntCfgPath);
    ASSERT_FALSE(stateLog.empty()) << "Pipeline produced no frames";

    // | Segment          | User-given frames | Sample frame |
    // |------------------|-------------------|--------------|
    // | load_game        | 0–57              | 20           |
    // | game_start       | 58–109            | 71           |
    // | cutscene_part_1  | 110–378           | 246          |
    // | starter_pick     | 379–469           | 399          |
    // | cutscene_part_2  | 470–832           | 807          |
    // | game_menu        | 833–866           | 837          |
    // | party_menu       | 867–909           | 882          |
    // | pokemon_summary  | 910–956           | 924          |
    // | resetting        | 957–1103          | 1000         |
    // | load_game (loop) | 1104–1189         | 1173         |
    //
    EXPECT_EQ(stateLog[20], "load_game") << "Frame 20: expected load_game";
    EXPECT_EQ(stateLog[71], "game_start") << "Frame 71: expected game_start";
    EXPECT_EQ(stateLog[246], "cutscene_part_1") << "Frame 246: expected cutscene_part_1";
    EXPECT_EQ(stateLog[399], "starter_pick") << "Frame 399: expected starter_pick";
    EXPECT_EQ(stateLog[807], "cutscene_part_2") << "Frame 807: expected cutscene_part_2";
    EXPECT_EQ(stateLog[837], "game_menu") << "Frame 837: expected game_menu";
    EXPECT_EQ(stateLog[882], "party_menu") << "Frame 882: expected party_menu";
    EXPECT_EQ(stateLog[924], "pokemon_summary") << "Frame 924: expected pokemon_summary";
    EXPECT_EQ(stateLog[1000], "resetting") << "Frame 1000: expected resetting";
    EXPECT_EQ(stateLog[1173], "load_game") << "Frame 1173: expected load_game (loop)";
}
