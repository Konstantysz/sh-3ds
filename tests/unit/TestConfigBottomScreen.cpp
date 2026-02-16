#include "Core/Config.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

    std::filesystem::path WriteTempYaml(const std::string &name, const std::string &content)
    {
        auto path = std::filesystem::temp_directory_path() / ("sh3ds_test_" + name + ".yaml");
        std::ofstream file(path);
        file << content;
        return path;
    }

} // namespace

TEST(ConfigBottomScreenTest, LoadsBottomScreenCalibration)
{
    std::string yaml = R"(
camera:
  type: "file"
  uri: "test_frames"
console:
  type: "luma3ds"
  ip: "127.0.0.1"
  port: 4950
screen_calibration:
  corners: [[100, 50], [540, 50], [540, 330], [100, 330]]
  target_width: 400
  target_height: 240
bottom_screen_calibration:
  corners: [[120, 220], [520, 220], [520, 420], [120, 420]]
  target_width: 320
  target_height: 240
orchestrator:
  target_fps: 12.0
  watchdog_timeout_s: 120
  dry_run: true
)";

    auto path = WriteTempYaml("bottom_screen", yaml);
    auto config = SH3DS::Core::LoadHardwareConfig(path.string());

    ASSERT_TRUE(config.bottomScreenCalibration.has_value());
    EXPECT_EQ(config.bottomScreenCalibration->targetWidth, 320);
    EXPECT_EQ(config.bottomScreenCalibration->targetHeight, 240);
    EXPECT_FLOAT_EQ(config.bottomScreenCalibration->corners[0].x, 120.0f);
    EXPECT_FLOAT_EQ(config.bottomScreenCalibration->corners[0].y, 220.0f);

    std::filesystem::remove(path);
}

TEST(ConfigBottomScreenTest, NoBottomScreenCalibrationIsNullopt)
{
    std::string yaml = R"(
camera:
  type: "file"
  uri: "test_frames"
console:
  type: "luma3ds"
  ip: "127.0.0.1"
  port: 4950
screen_calibration:
  corners: [[100, 50], [540, 50], [540, 330], [100, 330]]
  target_width: 400
  target_height: 240
orchestrator:
  target_fps: 12.0
  watchdog_timeout_s: 120
  dry_run: true
)";

    auto path = WriteTempYaml("no_bottom", yaml);
    auto config = SH3DS::Core::LoadHardwareConfig(path.string());

    EXPECT_FALSE(config.bottomScreenCalibration.has_value());

    std::filesystem::remove(path);
}

TEST(ConfigBottomScreenTest, BottomScreenDefaultDimensions)
{
    std::string yaml = R"(
camera:
  type: "file"
  uri: "test_frames"
console:
  type: "luma3ds"
  ip: "127.0.0.1"
  port: 4950
screen_calibration:
  corners: [[100, 50], [540, 50], [540, 330], [100, 330]]
  target_width: 400
  target_height: 240
bottom_screen_calibration:
  corners: [[120, 220], [520, 220], [520, 420], [120, 420]]
orchestrator:
  target_fps: 12.0
  watchdog_timeout_s: 120
  dry_run: true
)";

    auto path = WriteTempYaml("bottom_defaults", yaml);
    auto config = SH3DS::Core::LoadHardwareConfig(path.string());

    ASSERT_TRUE(config.bottomScreenCalibration.has_value());
    // Defaults should be 320x240 when not specified
    EXPECT_EQ(config.bottomScreenCalibration->targetWidth, 320);
    EXPECT_EQ(config.bottomScreenCalibration->targetHeight, 240);

    std::filesystem::remove(path);
}
