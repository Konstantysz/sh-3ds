#include "Core/Config.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

class ConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto tempDir = std::filesystem::temp_directory_path();

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);

        auto getTempPath = [&](const std::string &suffix) {
            std::stringstream ss;
            ss << "sh3ds_test_" << dis(gen) << suffix;
            return (tempDir / ss.str()).string();
        };

        hardwareYaml = getTempPath(".yaml");
        gameProfileYaml = getTempPath(".yaml");
        huntConfigYaml = getTempPath(".yaml");
        detectionProfileYaml = getTempPath(".yaml");
    }

    void TearDown() override
    {
        std::remove(hardwareYaml.c_str());
        std::remove(gameProfileYaml.c_str());
        std::remove(huntConfigYaml.c_str());
        std::remove(detectionProfileYaml.c_str());
    }

    void WriteFile(const std::string &path, const std::string &content)
    {
        std::ofstream out(path);
        out << content;
    }

    std::string hardwareYaml;
    std::string gameProfileYaml;
    std::string huntConfigYaml;
    std::string detectionProfileYaml;
};

TEST_F(ConfigTest, LoadHardwareConfig_ParsesCameraSection)
{
    WriteFile(hardwareYaml, R"(
camera:
  type: "mjpeg"
  uri: "http://192.168.1.100:8080/video"
  reconnect_delay_ms: 3000
  max_reconnect_attempts: 5
  grab_timeout_ms: 4000
console:
  type: "luma3ds"
  ip: "192.168.1.150"
  port: 4950
  default_hold_ms: 150
  default_release_ms: 100
  keepalive_interval_ms: 600
screen_calibration:
  target_width: 400
  target_height: 240
orchestrator:
  target_fps: 15.0
  watchdog_timeout_s: 90
  dry_run: true
  record_frames: false
  record_path: "./rec"
  log_level: "debug"
  log_file: "./logs/test.log"
  log_rotation_mb: 25
  log_max_files: 3
)");

    auto config = SH3DS::Core::LoadHardwareConfig(hardwareYaml);

    EXPECT_EQ(config.camera.type, "mjpeg");
    EXPECT_EQ(config.camera.uri, "http://192.168.1.100:8080/video");
    EXPECT_EQ(config.camera.reconnectDelayMs, 3000);
    EXPECT_EQ(config.camera.maxReconnectAttempts, 5);
    EXPECT_EQ(config.camera.grabTimeoutMs, 4000);

    EXPECT_EQ(config.console.type, "luma3ds");
    EXPECT_EQ(config.console.ip, "192.168.1.150");
    EXPECT_EQ(config.console.port, 4950);
    EXPECT_EQ(config.console.defaultHoldMs, 150);
    EXPECT_EQ(config.console.defaultReleaseMs, 100);

    EXPECT_EQ(config.screenCalibration.targetWidth, 400);
    EXPECT_EQ(config.screenCalibration.targetHeight, 240);

    EXPECT_DOUBLE_EQ(config.orchestrator.targetFps, 15.0);
    EXPECT_EQ(config.orchestrator.watchdogTimeoutS, 90);
    EXPECT_TRUE(config.orchestrator.dryRun);
    EXPECT_EQ(config.orchestrator.logLevel, "debug");
    EXPECT_EQ(config.orchestrator.logFile, "./logs/test.log");
}

TEST_F(ConfigTest, LoadHardwareConfig_UsesDefaults)
{
    WriteFile(hardwareYaml, R"(
camera:
  uri: "http://192.168.1.100:8080/video"
console:
  ip: "192.168.1.150"
)");

    auto config = SH3DS::Core::LoadHardwareConfig(hardwareYaml);

    EXPECT_EQ(config.camera.type, "mjpeg");
    EXPECT_EQ(config.camera.reconnectDelayMs, 2000);
    EXPECT_EQ(config.console.port, 4950);
    EXPECT_DOUBLE_EQ(config.orchestrator.targetFps, 12.0);
    EXPECT_FALSE(config.orchestrator.dryRun);
}

TEST_F(ConfigTest, LoadHardwareConfig_ThrowsOnMissingFile)
{
    EXPECT_THROW(SH3DS::Core::LoadHardwareConfig("/nonexistent/path.yaml"), std::runtime_error);
}

TEST_F(ConfigTest, LoadGameProfile_ParsesStatesAndRois)
{
    WriteFile(gameProfileYaml, R"(
game:
  id: "pokemon_xy"
  name: "Pokemon X / Pokemon Y"
rois:
  - name: "pokemon_sprite"
    x: 0.30
    y: 0.05
    w: 0.40
    h: 0.65
  - name: "dialogue_box"
    x: 0.02
    y: 0.72
    w: 0.96
    h: 0.26
states:
  initial: "unknown"
  debounce_frames: 3
  definitions:
    - id: "title_screen"
      description: "Main title screen"
      detection:
        method: "template_match"
        roi: "full_screen"
        template_path: "assets/templates/pokemon_xy/title_screen.png"
        threshold: 0.75
      transitions_to:
        - "intro_cutscene"
      max_duration_s: 30
      shiny_check: false
    - id: "starter_reveal"
      description: "Starter Pokemon reveal"
      detection:
        method: "color_histogram"
        roi: "pokemon_sprite"
        hsv_lower: [0, 0, 0]
        hsv_upper: [180, 255, 255]
        pixel_ratio_min: 0.0
        threshold: 0.5
      transitions_to:
        - "post_reveal"
      max_duration_s: 20
      shiny_check: true
)");

    auto profile = SH3DS::Core::LoadGameProfile(gameProfileYaml);

    EXPECT_EQ(profile.gameId, "pokemon_xy");
    EXPECT_EQ(profile.gameName, "Pokemon X / Pokemon Y");
    ASSERT_EQ(profile.rois.size(), 2u);
    EXPECT_EQ(profile.rois[0].name, "pokemon_sprite");
    EXPECT_DOUBLE_EQ(profile.rois[0].x, 0.30);
    EXPECT_DOUBLE_EQ(profile.rois[0].h, 0.65);

    EXPECT_EQ(profile.initialState, "unknown");
    EXPECT_EQ(profile.debounceFrames, 3);
    ASSERT_EQ(profile.states.size(), 2u);

    EXPECT_EQ(profile.states[0].id, "title_screen");
    EXPECT_EQ(profile.states[0].detection.method, "template_match");
    EXPECT_EQ(profile.states[0].detection.roi, "full_screen");
    EXPECT_FALSE(profile.states[0].shinyCheck);

    EXPECT_EQ(profile.states[1].id, "starter_reveal");
    EXPECT_TRUE(profile.states[1].shinyCheck);
    EXPECT_EQ(profile.states[1].detection.method, "color_histogram");
}

TEST_F(ConfigTest, LoadHuntConfig_ParsesActions)
{
    WriteFile(huntConfigYaml, R"(
hunt:
  id: "xy_starter_sr"
  name: "Pokemon X/Y Starter Soft Reset"
  game: "pokemon_xy"
  method: "soft_reset"
  target_pokemon: "froakie"
  detection_profile: "xy_froakie"
  shiny_check_state: "starter_reveal"
  shiny_check_frames: 15
  shiny_check_delay_ms: 1500
  on_shiny_action: "stop"
  alert:
    console_beep: true
    save_screenshot: true
    log_level: "error"
  recovery:
    on_stuck:
      action: "soft_reset"
      max_retries: 5
    on_detection_failure:
      action: "skip"
      max_consecutive: 10
  actions:
    title_screen:
      - buttons: ["A"]
        hold_ms: 150
        wait_after_ms: 2000
    intro_cutscene:
      - buttons: ["A"]
        hold_ms: 100
        wait_after_ms: 300
        repeat: true
    soft_reset:
      - buttons: ["L", "R", "START"]
        hold_ms: 500
        wait_after_ms: 3000
)");

    auto config = SH3DS::Core::LoadHuntConfig(huntConfigYaml);

    EXPECT_EQ(config.huntId, "xy_starter_sr");
    EXPECT_EQ(config.game, "pokemon_xy");
    EXPECT_EQ(config.targetPokemon, "froakie");
    EXPECT_EQ(config.shinyCheckState, "starter_reveal");
    EXPECT_EQ(config.shinyCheckFrames, 15);
    EXPECT_EQ(config.shinyCheckDelayMs, 1500);

    ASSERT_TRUE(config.actions.find("title_screen") != config.actions.end());
    ASSERT_EQ(config.actions.at("title_screen").size(), 1u);
    EXPECT_EQ(config.actions.at("title_screen")[0].buttons[0], "A");
    EXPECT_EQ(config.actions.at("title_screen")[0].holdMs, 150);

    ASSERT_TRUE(config.actions.find("soft_reset") != config.actions.end());
    EXPECT_EQ(config.actions.at("soft_reset")[0].buttons.size(), 3u);

    EXPECT_TRUE(config.alert.consoleBeep);
    EXPECT_EQ(config.onStuck.action, "soft_reset");
    EXPECT_EQ(config.onStuck.maxRetries, 5);
}

TEST_F(ConfigTest, LoadDetectionProfile_ParsesMethods)
{
    WriteFile(detectionProfileYaml, R"(
detection:
  profile_id: "xy_froakie"
  game: "pokemon_xy"
  pokemon: "froakie"
  methods:
    - method: "dominant_color"
      weight: 0.5
      normal_hsv_lower: [100, 100, 60]
      normal_hsv_upper: [130, 255, 200]
      shiny_hsv_lower: [95, 25, 170]
      shiny_hsv_upper: [135, 110, 255]
      shiny_ratio_threshold: 0.12
      normal_ratio_threshold: 0.12
    - method: "histogram_compare"
      weight: 0.3
      reference_normal: "assets/histograms/pokemon_xy/froakie_normal.yml"
      reference_shiny: "assets/histograms/pokemon_xy/froakie_shiny.yml"
      compare_method: "correlation"
      differential_threshold: 0.15
  fusion:
    shiny_threshold: 0.55
    uncertain_threshold: 0.35
)");

    auto profile = SH3DS::Core::LoadDetectionProfile(detectionProfileYaml);

    EXPECT_EQ(profile.profileId, "xy_froakie");
    EXPECT_EQ(profile.pokemon, "froakie");
    ASSERT_EQ(profile.methods.size(), 2u);

    EXPECT_EQ(profile.methods[0].method, "dominant_color");
    EXPECT_DOUBLE_EQ(profile.methods[0].weight, 0.5);
    EXPECT_DOUBLE_EQ(profile.methods[0].normalHsvLower[0], 100.0);

    EXPECT_EQ(profile.methods[1].method, "histogram_compare");
    EXPECT_EQ(profile.methods[1].referenceNormal, "assets/histograms/pokemon_xy/froakie_normal.yml");

    EXPECT_DOUBLE_EQ(profile.fusion.shinyThreshold, 0.55);
    EXPECT_DOUBLE_EQ(profile.fusion.uncertainThreshold, 0.35);
}
