#include "Config.h"

#include "Kappa/Logger.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace
{
    cv::Scalar ParseScalar(const YAML::Node &node)
    {
        if (!node.IsSequence() || node.size() < 3)
        {
            if (node.IsDefined() && !node.IsNull())
            {
                LOG_WARN("Config: expected HSV scalar sequence [h,s,v] but got '{}' — using (0,0,0)",
                    node.IsScalar() ? node.Scalar() : "<non-scalar>");
            }
            return cv::Scalar(0, 0, 0);
        }
        double v0 = node[0].as<double>(0.0);
        double v1 = node[1].as<double>(0.0);
        double v2 = node[2].as<double>(0.0);
        double v3 = node.size() > 3 ? node[3].as<double>(0.0) : 0.0;
        return cv::Scalar(v0, v1, v2, v3);
    }

} // namespace

namespace SH3DS::Core
{
    namespace
    {
        std::string ToLower(std::string value)
        {
            std::transform(value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        ScreenMode ParseScreenMode(const YAML::Node &node, const std::string &context)
        {
            if (!node || !node.IsScalar())
            {
                throw std::runtime_error(context
                                         + ": missing required scalar 'screen_mode' (expected 'single' or 'dual')");
            }

            const std::string mode = ToLower(node.as<std::string>(""));
            if (mode == "single")
            {
                return ScreenMode::Single;
            }
            if (mode == "dual")
            {
                return ScreenMode::Dual;
            }

            throw std::runtime_error(context + ": invalid screen_mode '" + node.as<std::string>("")
                                     + "' (expected 'single' or 'dual')");
        }

        cv::Scalar ParseRequiredScalar(const YAML::Node &node, const std::string &fieldPath)
        {
            if (!node || !node.IsSequence() || node.size() < 3)
            {
                throw std::runtime_error(fieldPath + ": expected sequence [h,s,v]");
            }

            return cv::Scalar(node[0].as<double>(0.0),
                node[1].as<double>(0.0),
                node[2].as<double>(0.0),
                node.size() > 3 ? node[3].as<double>(0.0) : 0.0);
        }

        void ValidateRoiDetectionParams(const RoiDetectionParams &params, const std::string &path)
        {
            const std::string method = ToLower(params.method);
            if (method != "color_histogram" && method != "pixel_ratio" && method != "template_match")
            {
                throw std::runtime_error(path + ": unknown method '" + params.method + "'");
            }

            if (params.roi.empty())
            {
                throw std::runtime_error(path + ": missing required field 'roi'");
            }

            if (params.threshold <= 0.0 || params.threshold > 1.0)
            {
                throw std::runtime_error(path + ": threshold must be in (0,1], got " + std::to_string(params.threshold));
            }

            if (method == "template_match")
            {
                if (params.templatePath.empty())
                {
                    throw std::runtime_error(path + ": template_match requires non-empty 'template_path'");
                }
            }
            else
            {
                if (params.pixelRatioMin < 0.0 || params.pixelRatioMin > 1.0 || params.pixelRatioMax < 0.0
                    || params.pixelRatioMax > 1.0)
                {
                    throw std::runtime_error(path + ": pixel_ratio_min/max must be in [0,1]");
                }
                if (params.pixelRatioMin > params.pixelRatioMax)
                {
                    throw std::runtime_error(path + ": pixel_ratio_min cannot exceed pixel_ratio_max");
                }

                for (int ch = 0; ch < 3; ++ch)
                {
                    if (params.hsvLower[ch] > params.hsvUpper[ch])
                    {
                        throw std::runtime_error(path + ": hsv_lower must be <= hsv_upper for each channel");
                    }
                }
            }
        }

        RoiDetectionParams ParseRoiDetectionParams(
            const YAML::Node &node, const std::string &stateId, const std::string &screen)
        {
            if (!node || !node.IsMap())
            {
                throw std::runtime_error("Config: fsm_states." + stateId + "." + screen + " must be a map");
            }

            const std::string path = "Config: fsm_states." + stateId + "." + screen;

            RoiDetectionParams params;
            params.roi = node["roi"].as<std::string>("");
            params.method = ToLower(node["method"].as<std::string>(""));
            params.templatePath = node["template_path"].as<std::string>("");
            params.threshold = node["threshold"].as<double>(0.7);
            params.pixelRatioMin = node["pixel_ratio_min"].as<double>(0.0);
            params.pixelRatioMax = node["pixel_ratio_max"].as<double>(1.0);

            if (params.method == "color_histogram" || params.method == "pixel_ratio")
            {
                if (!node["hsv_lower"] || !node["hsv_upper"])
                {
                    throw std::runtime_error(path + ": method '" + params.method
                                             + "' requires both 'hsv_lower' and 'hsv_upper'");
                }
                params.hsvLower = ParseRequiredScalar(node["hsv_lower"], path + ".hsv_lower");
                params.hsvUpper = ParseRequiredScalar(node["hsv_upper"], path + ".hsv_upper");
            }

            ValidateRoiDetectionParams(params, path);
            return params;
        }

        StateDetectionParams ParseStateDetectionParams(
            const YAML::Node &node, const std::string &stateId, ScreenMode screenMode)
        {
            if (!node || !node.IsMap())
            {
                throw std::runtime_error("Config: fsm_states." + stateId + " must be a map");
            }

            if (node["roi"] || node["method"] || node["hsv_lower"] || node["hsv_upper"] || node["pixel_ratio_min"]
                || node["pixel_ratio_max"] || node["threshold"] || node["template_path"] || node["rois"]
                || node["roi_mode"])
            {
                throw std::runtime_error("Config: fsm_states." + stateId
                                         + " uses legacy state-level detection fields. Use 'top'/'bottom' blocks.");
            }

            StateDetectionParams state;
            if (node["top"])
            {
                state.top = ParseRoiDetectionParams(node["top"], stateId, "top");
            }
            if (node["bottom"])
            {
                state.bottom = ParseRoiDetectionParams(node["bottom"], stateId, "bottom");
            }

            const bool hasTop = state.top.has_value();
            const bool hasBottom = state.bottom.has_value();
            if (screenMode == ScreenMode::Single)
            {
                if (hasTop == hasBottom)
                {
                    throw std::runtime_error("Config: fsm_states." + stateId
                                             + " in single mode must define exactly one of 'top' or 'bottom'");
                }
            }
            else
            {
                if (!hasTop && !hasBottom)
                {
                    throw std::runtime_error("Config: fsm_states." + stateId
                                             + " in dual mode must define at least one of 'top' or 'bottom'");
                }
            }

            return state;
        }
    } // namespace

    HardwareConfig LoadHardwareConfig(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load hardware config: " + std::string(e.what()));
        }

        HardwareConfig config;

        if (auto camera = root["camera"])
        {
            config.camera.type = camera["type"].as<std::string>(config.camera.type);
            config.camera.uri = camera["uri"].as<std::string>("");
            config.camera.reconnectDelayMs = camera["reconnect_delay_ms"].as<int>(config.camera.reconnectDelayMs);
            config.camera.maxReconnectAttempts =
                camera["max_reconnect_attempts"].as<int>(config.camera.maxReconnectAttempts);
            config.camera.grabTimeoutMs = camera["grab_timeout_ms"].as<int>(config.camera.grabTimeoutMs);
        }

        if (auto console = root["console"])
        {
            config.console.type = console["type"].as<std::string>(config.console.type);
            config.console.ip = console["ip"].as<std::string>("");
            config.console.port = console["port"].as<uint16_t>(config.console.port);
            config.console.defaultHoldMs = console["default_hold_ms"].as<int>(config.console.defaultHoldMs);
            config.console.defaultReleaseMs = console["default_release_ms"].as<int>(config.console.defaultReleaseMs);
            config.console.keepaliveIntervalMs =
                console["keepalive_interval_ms"].as<int>(config.console.keepaliveIntervalMs);
        }

        if (auto calib = root["screen_calibration"])
        {
            config.screenCalibration.targetWidth = calib["target_width"].as<int>(config.screenCalibration.targetWidth);
            config.screenCalibration.targetHeight =
                calib["target_height"].as<int>(config.screenCalibration.targetHeight);
        }

        if (auto bottomCalib = root["bottom_screen_calibration"])
        {
            ScreenCalibrationConfig bottom;
            bottom.targetWidth = bottomCalib["target_width"].as<int>(kBottomScreenWidth);
            bottom.targetHeight = bottomCalib["target_height"].as<int>(kBottomScreenHeight);
            config.bottomScreenCalibration = bottom;
        }

        if (auto orch = root["orchestrator"])
        {
            config.orchestrator.targetFps = orch["target_fps"].as<double>(config.orchestrator.targetFps);
            config.orchestrator.watchdogTimeoutS =
                orch["watchdog_timeout_s"].as<int>(config.orchestrator.watchdogTimeoutS);
            config.orchestrator.dryRun = orch["dry_run"].as<bool>(config.orchestrator.dryRun);
            config.orchestrator.recordFrames = orch["record_frames"].as<bool>(config.orchestrator.recordFrames);
            config.orchestrator.recordPath = orch["record_path"].as<std::string>(config.orchestrator.recordPath);
            config.orchestrator.logLevel = orch["log_level"].as<std::string>(config.orchestrator.logLevel);
            config.orchestrator.logFile = orch["log_file"].as<std::string>(config.orchestrator.logFile);
            config.orchestrator.logRotationMb = orch["log_rotation_mb"].as<int>(config.orchestrator.logRotationMb);
            config.orchestrator.logMaxFiles = orch["log_max_files"].as<int>(config.orchestrator.logMaxFiles);
        }

        return config;
    }

    GameProfile LoadGameProfile(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load game profile: " + std::string(e.what()));
        }

        GameProfile profile;

        if (auto game = root["game"])
        {
            profile.gameId = game["id"].as<std::string>("");
            profile.gameName = game["name"].as<std::string>("");
        }

        if (auto rois = root["rois"])
        {
            for (const auto &roiNode : rois)
            {
                RoiDefinition roi;
                roi.name = roiNode["name"].as<std::string>("");
                roi.x = roiNode["x"].as<double>(0.0);
                roi.y = roiNode["y"].as<double>(0.0);
                roi.w = roiNode["w"].as<double>(0.0);
                roi.h = roiNode["h"].as<double>(0.0);
                profile.rois.push_back(roi);
            }
        }

        if (auto states = root["states"])
        {
            profile.initialState = states["initial"].as<std::string>("unknown");
            profile.debounceFrames = states["debounce_frames"].as<int>(3);

            if (auto defs = states["definitions"])
            {
                for (const auto &defNode : defs)
                {
                    StateDefinition state;
                    state.id = defNode["id"].as<std::string>("");
                    state.description = defNode["description"].as<std::string>("");
                    state.maxDurationS = defNode["max_duration_s"].as<int>(60);
                    state.shinyCheck = defNode["shiny_check"].as<bool>(false);

                    if (auto det = defNode["detection"])
                    {
                        state.detection.method = det["method"].as<std::string>("");
                        state.detection.roi = det["roi"].as<std::string>("");
                        state.detection.templatePath = det["template_path"].as<std::string>("");
                        state.detection.threshold = det["threshold"].as<double>(0.7);
                        state.detection.pixelRatioMin = det["pixel_ratio_min"].as<double>(0.0);
                        state.detection.pixelRatioMax = det["pixel_ratio_max"].as<double>(1.0);

                        if (det["hsv_lower"])
                        {
                            state.detection.hsvLower = ParseScalar(det["hsv_lower"]);
                        }
                        if (det["hsv_upper"])
                        {
                            state.detection.hsvUpper = ParseScalar(det["hsv_upper"]);
                        }
                    }

                    if (auto transitions = defNode["transitions_to"])
                    {
                        for (const auto &t : transitions)
                        {
                            state.transitionsTo.push_back(t.as<std::string>());
                        }
                    }

                    profile.states.push_back(state);
                }
            }
        }

        return profile;
    }

    HuntDetectionParams LoadHuntDetectionParams(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load hunt detection params: " + std::string(e.what()));
        }

        HuntDetectionParams params;
        params.screenMode = ParseScreenMode(root["screen_mode"], "Config");
        params.debounceFrames = root["debounce_frames"].as<int>(3);
        if (params.debounceFrames < 1)
        {
            LOG_WARN("Config: debounce_frames ({}) < 1 — clamping to 1", params.debounceFrames);
            params.debounceFrames = 1;
        }

        if (auto states = root["states"])
        {
            for (auto it = states.begin(); it != states.end(); ++it)
            {
                std::string stateId = it->first.as<std::string>();
                auto node = it->second;
                params.stateParams[stateId] = ParseStateDetectionParams(node, stateId, params.screenMode);
            }
        }

        return params;
    }

    HuntConfig LoadHuntConfig(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load hunt config: " + std::string(e.what()));
        }

        HuntConfig config;

        if (auto hunt = root["hunt"])
        {
            config.huntId = hunt["id"].as<std::string>("");
            config.huntName = hunt["name"].as<std::string>("");
            config.game = hunt["game"].as<std::string>("");
            config.method = hunt["method"].as<std::string>("");
            config.targetPokemon = hunt["target_pokemon"].as<std::string>("");
            config.detectionProfile = hunt["detection_profile"].as<std::string>("");
            config.shinyCheckState = hunt["shiny_check_state"].as<std::string>("");
            config.shinyCheckFrames = hunt["shiny_check_frames"].as<int>(15);
            config.shinyCheckDelayMs = hunt["shiny_check_delay_ms"].as<int>(1500);
            config.onShinyAction = hunt["on_shiny_action"].as<std::string>("stop");

            if (auto alert = hunt["alert"])
            {
                config.alert.consoleBeep = alert["console_beep"].as<bool>(true);
                config.alert.saveScreenshot = alert["save_screenshot"].as<bool>(true);
                config.alert.logLevel = alert["log_level"].as<std::string>("error");
            }

            if (auto recovery = hunt["recovery"])
            {
                if (auto onStuck = recovery["on_stuck"])
                {
                    config.onStuck.action = onStuck["action"].as<std::string>("soft_reset");
                    config.onStuck.maxRetries = onStuck["max_retries"].as<int>(5);
                }
                if (auto onFail = recovery["on_detection_failure"])
                {
                    config.onDetectionFailure.action = onFail["action"].as<std::string>("skip");
                    config.onDetectionFailure.maxConsecutive = onFail["max_consecutive"].as<int>(10);
                }
            }

            if (auto actions = hunt["actions"])
            {
                for (auto it = actions.begin(); it != actions.end(); ++it)
                {
                    std::string stateName = it->first.as<std::string>();
                    std::vector<InputAction> stateActions;

                    for (const auto &actionNode : it->second)
                    {
                        InputAction action;
                        if (actionNode["buttons"])
                        {
                            for (const auto &btn : actionNode["buttons"])
                            {
                                action.buttons.push_back(btn.as<std::string>());
                            }
                        }
                        action.holdMs = actionNode["hold_ms"].as<int>(120);
                        action.waitAfterMs = actionNode["wait_after_ms"].as<int>(200);
                        action.repeat = actionNode["repeat"].as<bool>(false);
                        action.waitMs = actionNode["wait_ms"].as<int>(0);
                        stateActions.push_back(action);
                    }

                    config.actions[stateName] = stateActions;
                }
            }
        }

        return config;
    }

    UnifiedHuntConfig LoadUnifiedHuntConfig(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load unified hunt config: " + std::string(e.what()));
        }

        UnifiedHuntConfig config;

        // Identity
        config.huntId = root["hunt_id"].as<std::string>("");
        config.huntName = root["hunt_name"].as<std::string>("");
        config.targetPokemon = root["target_pokemon"].as<std::string>("");
        config.screenMode = ParseScreenMode(root["screen_mode"], "Config");

        // ROIs
        if (auto rois = root["rois"])
        {
            for (const auto &roiNode : rois)
            {
                RoiDefinition roi;
                roi.name = roiNode["name"].as<std::string>("");
                roi.x = roiNode["x"].as<double>(0.0);
                roi.y = roiNode["y"].as<double>(0.0);
                roi.w = roiNode["w"].as<double>(0.0);
                roi.h = roiNode["h"].as<double>(0.0);
                config.rois.push_back(roi);
            }
        }

        // FSM detection params
        config.fsmParams.screenMode = config.screenMode;
        config.fsmParams.debounceFrames = root["debounce_frames"].as<int>(3);
        if (config.fsmParams.debounceFrames < 1)
        {
            LOG_WARN("Config: debounce_frames ({}) < 1 — clamping to 1", config.fsmParams.debounceFrames);
            config.fsmParams.debounceFrames = 1;
        }
        if (auto states = root["fsm_states"])
        {
            for (auto it = states.begin(); it != states.end(); ++it)
            {
                std::string stateId = it->first.as<std::string>();
                auto node = it->second;
                config.fsmParams.stateParams[stateId] =
                    ParseStateDetectionParams(node, stateId, config.fsmParams.screenMode);
            }
        }

        // Shiny detector (single method)
        if (auto det = root["shiny_detector"])
        {
            config.shinyDetector.method = det["method"].as<std::string>("");
            config.shinyDetector.roi = det["roi"].as<std::string>("");
            config.shinyDetector.weight = det["weight"].as<double>(1.0);

            if (det["normal_hsv_lower"])
            {
                config.shinyDetector.normalHsvLower = ParseScalar(det["normal_hsv_lower"]);
            }
            if (det["normal_hsv_upper"])
            {
                config.shinyDetector.normalHsvUpper = ParseScalar(det["normal_hsv_upper"]);
            }
            if (det["shiny_hsv_lower"])
            {
                config.shinyDetector.shinyHsvLower = ParseScalar(det["shiny_hsv_lower"]);
            }
            if (det["shiny_hsv_upper"])
            {
                config.shinyDetector.shinyHsvUpper = ParseScalar(det["shiny_hsv_upper"]);
            }
            config.shinyDetector.shinyRatioThreshold = det["shiny_ratio_threshold"].as<double>(0.12);
            config.shinyDetector.normalRatioThreshold = det["normal_ratio_threshold"].as<double>(0.12);

            config.shinyDetector.referenceNormal = det["reference_normal"].as<std::string>("");
            config.shinyDetector.referenceShiny = det["reference_shiny"].as<std::string>("");
            config.shinyDetector.compareMethod = det["compare_method"].as<std::string>("correlation");
            config.shinyDetector.differentialThreshold = det["differential_threshold"].as<double>(0.15);
            config.shinyDetector.sparkleRoi = det["sparkle_roi"].as<std::string>("sparkle_region");
            config.shinyDetector.brightnessThreshold = det["brightness_threshold"].as<int>(240);
            config.shinyDetector.minBrightPixelRatio = det["min_bright_pixel_ratio"].as<double>(0.005);
            config.shinyDetector.minConsecutiveFrames = det["min_consecutive_frames"].as<int>(3);
        }

        // Fusion
        if (auto fusion = root["fusion"])
        {
            config.fusion.shinyThreshold = fusion["shiny_threshold"].as<double>(0.55);
            config.fusion.uncertainThreshold = fusion["uncertain_threshold"].as<double>(0.35);
        }

        // Input actions per state
        if (auto actions = root["actions"])
        {
            for (auto it = actions.begin(); it != actions.end(); ++it)
            {
                std::string stateName = it->first.as<std::string>();
                std::vector<InputAction> stateActions;

                for (const auto &actionNode : it->second)
                {
                    InputAction action;
                    if (actionNode["buttons"])
                    {
                        for (const auto &btn : actionNode["buttons"])
                        {
                            action.buttons.push_back(btn.as<std::string>());
                        }
                    }
                    action.holdMs = actionNode["hold_ms"].as<int>(120);
                    action.waitAfterMs = actionNode["wait_after_ms"].as<int>(200);
                    action.repeat = actionNode["repeat"].as<bool>(false);
                    action.waitMs = actionNode["wait_ms"].as<int>(0);
                    stateActions.push_back(action);
                }

                config.actions[stateName] = stateActions;
            }
        }

        // Hunt behaviour
        config.shinyCheckState = root["shiny_check_state"].as<std::string>("");
        config.shinyCheckFrames = root["shiny_check_frames"].as<int>(15);
        config.shinyCheckDelayMs = root["shiny_check_delay_ms"].as<int>(1500);
        config.onShinyAction = root["on_shiny_action"].as<std::string>("stop");

        if (auto alert = root["alert"])
        {
            config.alert.consoleBeep = alert["console_beep"].as<bool>(true);
            config.alert.saveScreenshot = alert["save_screenshot"].as<bool>(true);
            config.alert.logLevel = alert["log_level"].as<std::string>("error");
        }

        if (auto recovery = root["recovery"])
        {
            if (auto onStuck = recovery["on_stuck"])
            {
                config.onStuck.action = onStuck["action"].as<std::string>("soft_reset");
                config.onStuck.maxRetries = onStuck["max_retries"].as<int>(5);
            }
            if (auto onFail = recovery["on_detection_failure"])
            {
                config.onDetectionFailure.action = onFail["action"].as<std::string>("skip");
                config.onDetectionFailure.maxConsecutive = onFail["max_consecutive"].as<int>(10);
            }
        }

        return config;
    }

    HuntConfig ToHuntConfig(const UnifiedHuntConfig &unified)
    {
        HuntConfig config;
        config.huntId = unified.huntId;
        config.huntName = unified.huntName;
        config.targetPokemon = unified.targetPokemon;
        config.shinyCheckState = unified.shinyCheckState;
        config.shinyCheckFrames = unified.shinyCheckFrames;
        config.shinyCheckDelayMs = unified.shinyCheckDelayMs;
        config.onShinyAction = unified.onShinyAction;
        config.actions = unified.actions;
        config.alert = unified.alert;
        config.onStuck = unified.onStuck;
        config.onDetectionFailure = unified.onDetectionFailure;
        return config;
    }

    DetectionProfile LoadDetectionProfile(const std::string &path)
    {
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path);
        }
        catch (const YAML::Exception &e)
        {
            throw std::runtime_error("Failed to load detection profile: " + std::string(e.what()));
        }

        DetectionProfile profile;

        if (auto det = root["detection"])
        {
            profile.profileId = det["profile_id"].as<std::string>("");
            profile.game = det["game"].as<std::string>("");
            profile.pokemon = det["pokemon"].as<std::string>("");

            if (auto methods = det["methods"])
            {
                for (const auto &methodNode : methods)
                {
                    DetectionMethodConfig method;
                    method.method = methodNode["method"].as<std::string>("");
                    method.weight = methodNode["weight"].as<double>(1.0);

                    // dominant_color params
                    if (methodNode["normal_hsv_lower"])
                    {
                        method.normalHsvLower = ParseScalar(methodNode["normal_hsv_lower"]);
                    }
                    if (methodNode["normal_hsv_upper"])
                    {
                        method.normalHsvUpper = ParseScalar(methodNode["normal_hsv_upper"]);
                    }
                    if (methodNode["shiny_hsv_lower"])
                    {
                        method.shinyHsvLower = ParseScalar(methodNode["shiny_hsv_lower"]);
                    }
                    if (methodNode["shiny_hsv_upper"])
                    {
                        method.shinyHsvUpper = ParseScalar(methodNode["shiny_hsv_upper"]);
                    }
                    method.shinyRatioThreshold = methodNode["shiny_ratio_threshold"].as<double>(0.12);
                    method.normalRatioThreshold = methodNode["normal_ratio_threshold"].as<double>(0.12);

                    // histogram_compare params
                    method.referenceNormal = methodNode["reference_normal"].as<std::string>("");
                    method.referenceShiny = methodNode["reference_shiny"].as<std::string>("");
                    method.compareMethod = methodNode["compare_method"].as<std::string>("correlation");
                    method.differentialThreshold = methodNode["differential_threshold"].as<double>(0.15);

                    // sparkle params
                    method.sparkleRoi = methodNode["sparkle_roi"].as<std::string>("sparkle_region");
                    method.brightnessThreshold = methodNode["brightness_threshold"].as<int>(240);
                    method.minBrightPixelRatio = methodNode["min_bright_pixel_ratio"].as<double>(0.005);
                    method.minConsecutiveFrames = methodNode["min_consecutive_frames"].as<int>(3);

                    profile.methods.push_back(method);
                }
            }

            if (auto fusion = det["fusion"])
            {
                profile.fusion.shinyThreshold = fusion["shiny_threshold"].as<double>(0.55);
                profile.fusion.uncertainThreshold = fusion["uncertain_threshold"].as<double>(0.35);
            }
        }

        return profile;
    }

} // namespace SH3DS::Core




