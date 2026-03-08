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

/// Integration test: runs the real pipeline against the Fennekin video replay
/// and asserts the FSM reaches expected states at safe midpoint frames.
///
/// All config comes from YAML — no hardcoded detection params.
/// Pipeline mirrors Orchestrator::MainLoopTick exactly.
TEST(XYStarterFennekinReplay, StateSequenceMatchesExpected)
{
    const std::filesystem::path kRepo = SH3DS_REPO_ROOT;
    const auto imagesPath = kRepo / "video_replays/fennekin_normal/images";
    const auto huntCfgPath = (kRepo / "config/hunts/xy_starter_sr_fennekin.yaml").string();

    // Skip test gracefully if replay frames are not present
    if (!std::filesystem::exists(imagesPath) || std::filesystem::is_empty(imagesPath))
    {
        GTEST_SKIP() << "Replay frames not found at " << imagesPath << " — skipping video replay test.";
    }

    // 1. Load config from YAML
    auto unified = SH3DS::Core::LoadUnifiedHuntConfig(huntCfgPath);

    // 2. Frame source
    auto frameSource = SH3DS::Capture::FileFrameSource::CreateFileFrameSource(imagesPath, 0.0);
    ASSERT_NE(frameSource, nullptr);
    ASSERT_TRUE(frameSource->Open()) << "Failed to open frame source at " << imagesPath;

    // 3. Screen detector (auto-calibrates from first ~15 frames)
    auto screenDetector = SH3DS::Capture::ScreenDetector::CreateScreenDetector();

    // 4. Frame preprocessor — zeroed top corners (ScreenDetector populates them at runtime)
    SH3DS::Core::ScreenCalibrationConfig topCalib; // corners default to all-zero
    SH3DS::Capture::FramePreprocessor preprocessor(topCalib, unified.rois, std::nullopt);

    // 5. FSM — built from YAML params, same factory as real app
    auto fsm = SH3DS::FSM::HuntProfiles::CreateXYStarterSR(unified.fsmParams);
    ASSERT_NE(fsm, nullptr);

    // 6. Per-frame loop (mirrors Orchestrator::MainLoopTick)
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
            // Screen not yet detected — keep last known state
            stateLog.push_back(lastState);
            continue;
        }

        // Apply color correction to the full warped top image before ROI extraction so that
        // Gray World WB has the complete scene to compute balanced channel gains.
        // Bottom screen is LCD-rendered UI — WB correction is not applied there.
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
    // | load_game (loop) | 512–604           | 560          |
    //
    EXPECT_EQ(stateLog[15],  "load_game")       << "Frame 15: expected load_game";
    EXPECT_EQ(stateLog[42],  "game_start")      << "Frame 42: expected game_start";
    EXPECT_EQ(stateLog[120], "cutscene_part_1") << "Frame 120: expected cutscene_part_1";
    EXPECT_EQ(stateLog[220], "starter_pick")    << "Frame 220: expected starter_pick";
    EXPECT_EQ(stateLog[350], "cutscene_part_2") << "Frame 350: expected cutscene_part_2";
    EXPECT_EQ(stateLog[464], "game_menu")       << "Frame 464: expected game_menu";
    EXPECT_EQ(stateLog[481], "party_menu")      << "Frame 481: expected party_menu";
    EXPECT_EQ(stateLog[534], "pokemon_summary") << "Frame 534: expected pokemon_summary";
    EXPECT_EQ(stateLog[590], "load_game")       << "Frame 590: expected load_game (loop)";
}
