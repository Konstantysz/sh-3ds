#pragma once

#include "Constants.h"

#include <opencv2/core.hpp>

#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SH3DS::Core
{

    /**
     * @brief Camera source configuration.
     */
    struct CameraConfig
    {
        std::string type = "mjpeg"; ///< Type of camera: "mjpeg" for MJPEG stream, "usb" for USB camera
        std::string uri; ///< URI for MJPEG stream (e.g., "http://[IP_ADDRESS]/stream") or device index for USB camera
        int reconnectDelayMs = 2000;   ///< Delay between reconnection attempts in milliseconds
        int maxReconnectAttempts = 10; ///< Maximum number of reconnection attempts
        int grabTimeoutMs = 5000;      ///< Timeout for grabbing a frame in milliseconds
    };

    /**
     * @brief 3DS console connection configuration.
     */
    struct ConsoleConfig
    {
        std::string type =
            "luma3ds";  ///< Type of console connection: "luma3ds" for Luma3DS remote play, "citra" for Citra emulator
        std::string ip; ///< IP address of the 3DS console or emulator
        uint16_t port = 4950;          ///< Port for console connection
        int defaultHoldMs = 120;       ///< Default hold time for buttons in milliseconds
        int defaultReleaseMs = 80;     ///< Default release time for buttons in milliseconds
        int keepaliveIntervalMs = 500; ///< Keep-alive interval in milliseconds
    };

    /**
     * @brief Screen calibration (fixed corners for perspective warp).
     */
    struct ScreenCalibrationConfig
    {
        std::array<cv::Point2f, 4> corners = {}; ///< Four corners of the screen in source image coordinates
                                                 ///< (TL, TR, BR, BL). Defaults to all-zero; populated at
                                                 ///< runtime by ScreenDetector auto-calibration.
        int targetWidth = kTopScreenWidth;       ///< Target width for warped image
        int targetHeight = kTopScreenHeight;     ///< Target height for warped image
    };

    /**
     * @brief Orchestrator runtime configuration.
     */
    struct OrchestratorConfig
    {
        double targetFps = 12.0;                 ///< Target frames per second for the orchestrator
        int watchdogTimeoutS = 120;              ///< Watchdog timeout in seconds
        bool dryRun = false;                     ///< Whether to run in dry-run mode (no actual actions)
        bool recordFrames = false;               ///< Whether to record frames
        std::string recordPath = "./recordings"; ///< Path to save recorded frames
        std::string logLevel = "info";           ///< Log level
        std::string logFile;                     ///< Path to log file
        int logRotationMb = 50;                  ///< Log rotation size in megabytes
        int logMaxFiles = 5;                     ///< Maximum number of log files
        std::string shinyRoi = "pokemon_sprite"; ///< ROI name used for shiny detection (from hunt config)
    };

    /**
     * @brief Top-level hardware configuration (loaded from hardware.yaml).
     */
    struct HardwareConfig
    {
        CameraConfig camera;                                            ///< Camera configuration
        ConsoleConfig console;                                          ///< Console configuration
        ScreenCalibrationConfig screenCalibration;                      ///< Top screen calibration
        std::optional<ScreenCalibrationConfig> bottomScreenCalibration; ///< Bottom screen calibration (optional)
        OrchestratorConfig orchestrator;                                ///< Orchestrator configuration
    };

    /**
     * @brief ROI definition (normalized coordinates 0.0 - 1.0).
     */
    struct RoiDefinition
    {
        std::string name; ///< Name of the ROI
        double x = 0.0;   ///< X coordinate of the top-left corner (normalized 0.0 - 1.0)
        double y = 0.0;   ///< Y coordinate of the top-left corner (normalized 0.0 - 1.0)
        double w = 0.0;   ///< Width of the ROI (normalized 0.0 - 1.0)
        double h = 0.0;   ///< Height of the ROI (normalized 0.0 - 1.0)
    };

    /**
     * @brief State detection rule for the config-driven FSM.
     */
    struct StateDetectionRule
    {
        std::string method;         ///< Detection method: "template", "hsv", "ratio"
        std::string roi;            ///< Name of the ROI to apply detection to
        std::string templatePath;   ///< Path to template image for template matching
        cv::Scalar hsvLower;        ///< Lower HSV bounds for color detection
        cv::Scalar hsvUpper;        ///< Upper HSV bounds for color detection
        double pixelRatioMin = 0.0; ///< Minimum pixel ratio for detection
        double pixelRatioMax = 1.0; ///< Maximum pixel ratio for detection
        double threshold = 0.7;     ///< Threshold for detection
    };

    /**
     * @brief Game state definition.
     */
    struct StateDefinition
    {
        std::string id;                         ///< Unique identifier for the state
        std::string description;                ///< Description of the state
        StateDetectionRule detection;           ///< State detection rule
        std::vector<std::string> transitionsTo; ///< List of states that can be transitioned to from this state
        int maxDurationS = 60;                  ///< Maximum duration of the state in seconds
        bool shinyCheck = false;                ///< Whether to perform shiny check in this state
    };

    /**
     * @brief Game profile (loaded from games/&lt;game&gt;.yaml).
     */
    struct GameProfile
    {
        std::string gameId;                   ///< Unique identifier for the game
        std::string gameName;                 ///< Name of the game
        std::vector<RoiDefinition> rois;      ///< List of ROIs for the game
        std::string initialState = "unknown"; ///< Initial state of the game
        std::vector<StateDefinition> states;  ///< List of states for the game
        int debounceFrames = 3;               ///< Number of frames to debounce state transitions
    };

    /**
     * @brief Single input action in a hunt strategy.
     */
    struct InputAction
    {
        std::vector<std::string> buttons; ///< List of buttons to press
        int holdMs = 120;                 ///< Hold time for buttons in milliseconds
        int waitAfterMs = 200;            ///< Wait time after releasing buttons in milliseconds
        bool repeat = false;              ///< Whether to repeat the action
        int waitMs = 0;                   ///< Wait time before repeating the action in milliseconds
    };

    /**
     * @brief Recovery policy.
     */
    struct RecoveryPolicy
    {
        std::string action = "soft_reset"; ///< Action to take on recovery
        int maxRetries = 5;                ///< Maximum number of retries
        int maxConsecutive = 10;           ///< Maximum number of consecutive recoveries
    };

    /**
     * @brief Alert configuration on shiny found.
     */
    struct AlertConfig
    {
        bool consoleBeep = true;        ///< Whether to beep the console
        bool saveScreenshot = true;     ///< Whether to save screenshot on shiny found
        std::string logLevel = "error"; ///< Log level for alerts
    };

    /**
     * @brief Single detection method configuration.
     */
    struct DetectionMethodConfig
    {
        std::string method;                        ///< Detection method
        std::string roi;                           ///< ROI name to run detection on
        double weight = 1.0;                       ///< Weight for this detection method
        cv::Scalar normalHsvLower;                 ///< Lower HSV bounds for normal detection
        cv::Scalar normalHsvUpper;                 ///< Upper HSV bounds for normal detection
        cv::Scalar shinyHsvLower;                  ///< Lower HSV bounds for shiny detection
        cv::Scalar shinyHsvUpper;                  ///< Upper HSV bounds for shiny detection
        double shinyRatioThreshold = 0.12;         ///< Threshold for shiny detection
        double normalRatioThreshold = 0.12;        ///< Threshold for normal detection
        std::string referenceNormal;               ///< Path to reference normal image
        std::string referenceShiny;                ///< Path to reference shiny image
        std::string compareMethod = "correlation"; ///< Comparison method
        double differentialThreshold = 0.15;       ///< Differential threshold
        std::string sparkleRoi = "sparkle_region"; ///< ROI for sparkle detection
        int brightnessThreshold = 240;             ///< Brightness threshold
        double minBrightPixelRatio = 0.005;        ///< Minimum pixel ratio for brightness detection
        int minConsecutiveFrames = 3;              ///< Minimum number of consecutive frames for detection
    };

    /**
     * @brief Fusion configuration for multi-method detection.
     */
    struct FusionConfig
    {
        double shinyThreshold = 0.55;     ///< Threshold for shiny detection
        double uncertainThreshold = 0.35; ///< Threshold for uncertain detection
    };

    /**
     * @brief Detection profile (loaded from detection/&lt;profile&gt;.yaml).
     */
    struct DetectionProfile
    {
        std::string profileId;                      ///< Unique identifier for the detection profile
        std::string game;                           ///< Game for which the detection profile is defined
        std::string pokemon;                        ///< Pokémon for which the detection profile is defined
        std::vector<DetectionMethodConfig> methods; ///< List of detection methods
        FusionConfig fusion;                        ///< Fusion configuration for multi-method detection
    };

    /**
     * @brief Hunt strategy configuration (loaded from hunts/&lt;hunt&gt;.yaml).
     */
    struct HuntConfig
    {
        std::string huntId;                                      ///< Unique identifier for the hunt
        std::string huntName;                                    ///< Name of the hunt
        std::string game;                                        ///< Game for which the hunt is defined
        std::string method;                                      ///< Method used for the hunt
        std::string targetPokemon;                               ///< Target Pokémon for the hunt
        std::string detectionProfile;                            ///< Detection profile to use for the hunt
        std::map<std::string, std::vector<InputAction>> actions; ///< Map of actions to perform
        std::string shinyCheckState;                             ///< State to check for shiny
        int shinyCheckFrames = 15;                               ///< Number of frames to check for shiny
        int shinyCheckDelayMs = 1500;                            ///< Delay before checking for shiny
        std::string onShinyAction = "stop";                      ///< Action to take on shiny found
        AlertConfig alert;                                       ///< Alert configuration
        RecoveryPolicy onStuck;                                  ///< Recovery policy for stuck state
        RecoveryPolicy onDetectionFailure;                       ///< Recovery policy for detection failure
    };

    /**
     * @brief Screen mode for state detection input.
     */
    enum class ScreenMode
    {
        Single, ///< Single-screen device/config (exactly one ROI block per state)
        Dual,   ///< Dual-screen device/config (top and/or bottom ROI blocks per state)
    };

    /**
     * @brief Detection parameters for a single ROI block (top or bottom).
     */
    struct RoiDetectionParams
    {
        std::string roi;            ///< ROI name to analyze
        std::string method;         ///< Detection method: "color_histogram", "pixel_ratio", "template_match"
        cv::Scalar hsvLower;        ///< Lower HSV bounds
        cv::Scalar hsvUpper;        ///< Upper HSV bounds
        double pixelRatioMin = 0.0; ///< Minimum pixel ratio
        double pixelRatioMax = 1.0; ///< Maximum pixel ratio
        double threshold = 0.7;     ///< Confidence threshold
        std::string templatePath;   ///< Path to template image (for template_match)
    };

    /**
     * @brief Per-state detection parameters loaded from YAML for tuning.
     */
    struct StateDetectionParams
    {
        std::optional<RoiDetectionParams> top;    ///< Optional top-screen ROI detection block
        std::optional<RoiDetectionParams> bottom; ///< Optional bottom-screen ROI detection block
    };

    /**
     * @brief Detection parameters for a complete hunt, keyed by state ID.
     */
    struct HuntDetectionParams
    {
        ScreenMode screenMode = ScreenMode::Single;              ///< Device/config screen mode
        std::map<std::string, StateDetectionParams> stateParams; ///< Detection params per state
        int debounceFrames = 3;                                  ///< Frame debounce count
    };

    /**
     * @brief All configuration needed to run a single hunt, loaded from one YAML file.
     *
     * Replaces the old combination of game profile + hunt config + detection profile
     * + detection params. Hardware (camera, console, screen warp) stays in hardware.yaml.
     */
    struct UnifiedHuntConfig
    {
        // Identity
        std::string huntId;                         ///< Unique identifier for this hunt config
        std::string huntName;                       ///< Human-readable name
        std::string targetPokemon;                  ///< Target Pokémon
        ScreenMode screenMode = ScreenMode::Single; ///< Single vs dual-screen detection mode

        // ROIs (fed to FramePreprocessor)
        std::vector<RoiDefinition> rois; ///< Named regions of interest

        // FSM detection params (fed to HuntProfiles factory)
        HuntDetectionParams fsmParams; ///< Per-state HSV/threshold params + debounce

        // Shiny detector params
        DetectionMethodConfig shinyDetector; ///< Dominant-color or histogram detector config
        FusionConfig fusion;                 ///< Fusion thresholds for multi-method

        // Input actions per state
        std::map<std::string, std::vector<InputAction>> actions; ///< Buttons to press on each state

        // Hunt behaviour
        std::string shinyCheckState;        ///< State that triggers shiny detection
        int shinyCheckFrames = 15;          ///< Frames to sample during shiny check
        int shinyCheckDelayMs = 1500;       ///< Delay before sampling
        std::string onShinyAction = "stop"; ///< "stop" or "continue"
        AlertConfig alert;                  ///< Alert settings
        RecoveryPolicy onStuck;             ///< Stuck-state recovery policy
        RecoveryPolicy onDetectionFailure;  ///< Detection-failure recovery policy
    };

    /**
     * @brief Load hardware configuration from YAML file.
     * @param path Path to the hardware configuration file.
     * @return Hardware configuration.
     */
    HardwareConfig LoadHardwareConfig(const std::string &path);

    /**
     * @brief Load game profile from YAML file.
     * @param path Path to the game profile file.
     * @return Game profile.
     */
    GameProfile LoadGameProfile(const std::string &path);

    /**
     * @brief Load hunt configuration from YAML file.
     * @param path Path to the hunt configuration file.
     * @return Hunt configuration.
     */
    HuntConfig LoadHuntConfig(const std::string &path);

    /**
     * @brief Load detection profile from YAML file.
     * @param path Path to the detection profile file.
     * @return Detection profile.
     */
    DetectionProfile LoadDetectionProfile(const std::string &path);

    /**
     * @brief Load hunt detection parameters from YAML file.
     * @param path Path to the detection parameters file.
     * @return Hunt detection parameters.
     */
    HuntDetectionParams LoadHuntDetectionParams(const std::string &path);

    /**
     * @brief Load a unified hunt configuration from a single YAML file.
     * @param path Path to the hunt config file.
     * @return Unified hunt configuration.
     */
    UnifiedHuntConfig LoadUnifiedHuntConfig(const std::string &path);

    /**
     * @brief Extract a HuntConfig from a UnifiedHuntConfig (for SoftResetStrategy compatibility).
     * @param unified Source unified config.
     * @return HuntConfig with actions, recovery policy, and behaviour fields populated.
     */
    HuntConfig ToHuntConfig(const UnifiedHuntConfig &unified);

} // namespace SH3DS::Core
