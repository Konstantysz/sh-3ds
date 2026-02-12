#pragma once

#include "Input/InputCommand.h"

#include <opencv2/core.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SH3DS::Core
{
    /**
     * @brief Game state identifier (string-based for config-driven FSM).
     */
    using GameState = std::string;

    /**
     * @brief Named set of regions of interest extracted from a preprocessed frame.
     */
    using ROISet = std::map<std::string, cv::Mat>;

    /**
     * @brief Metadata attached to every captured frame.
     */
    struct FrameMetadata
    {
        uint64_t sequenceNumber = 0;                            ///< Sequence number of the frame
        std::chrono::steady_clock::time_point captureTime = {}; ///< Time when the frame was captured
        int sourceWidth = 0;                                    ///< Width of the source image
        int sourceHeight = 0;                                   ///< Height of the source image
        double fpsEstimate = 0.0;                               ///< Estimated frames per second
    };

    /**
     * @brief A single captured frame with metadata.
     */
    struct Frame
    {
        cv::Mat image;          ///< The image data
        FrameMetadata metadata; ///< Metadata associated with the frame
    };

    /**
     * @brief Verdict from shiny detection.
     */
    enum class ShinyVerdict
    {
        NotShiny,
        Shiny,
        Uncertain,
    };

    /**
     * @brief Result of a shiny detection analysis.
     */
    struct ShinyResult
    {
        ShinyVerdict verdict = ShinyVerdict::Uncertain; ///< Verdict from shiny detection
        double confidence = 0.0;                        ///< Confidence level of the verdict
        std::string method;                             ///< Method used for detection
        std::string details;                            ///< Additional details about the detection
        cv::Mat debugImage;                             ///< Debug image showing the detection
    };

    /**
     * @brief Record of a state transition.
     */
    struct StateTransition
    {
        GameState from;                                  ///< Previous state
        GameState to;                                    ///< New state
        std::chrono::steady_clock::time_point timestamp; ///< Timestamp of the transition
    };

    /**
     * @brief What action the hunt strategy wants to take.
     */
    enum class HuntAction
    {
        Wait,
        SendInput,
        CheckShiny,
        AlertShiny,
        Reset,
        Abort,
    };

    /**
     * @brief Decision produced by HuntStrategy::Tick().
     */
    struct HuntDecision
    {
        HuntAction action = HuntAction::Wait;                           ///< Action to take
        Input::InputCommand input;                                      ///< Input command to send
        std::string reason;                                             ///< Reason for the decision
        std::chrono::milliseconds delay = std::chrono::milliseconds(0); ///< Delay before next action
    };

    /**
     * @brief Accumulated hunt statistics.
     */
    struct HuntStatistics
    {
        uint64_t encounters = 0;                                  ///< Number of encounters
        uint64_t shiniesFound = 0;                                ///< Number of shinies found
        std::chrono::steady_clock::time_point huntStarted = {};   ///< Time when the hunt started
        std::chrono::steady_clock::time_point lastEncounter = {}; ///< Time of the last encounter
        double avgCycleSeconds = 0.0;                             ///< Average time per cycle in seconds
        uint64_t errors = 0;                                      ///< Number of errors
        uint64_t watchdogRecoveries = 0;                          ///< Number of watchdog recoveries
    };
} // namespace SH3DS::Core
