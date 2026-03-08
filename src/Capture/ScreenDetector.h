#pragma once

#include "Core/Constants.h"

#include <opencv2/core.hpp>

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace SH3DS::Capture
{
    class FramePreprocessor; // Forward declaration for ApplyTo

    /**
     * @brief A single detected screen with corner positions and confidence.
     */
    struct DetectedScreen
    {
        std::array<cv::Point2f, 4> corners; ///< Ordered: TL, TR, BR, BL
        double confidence = 0.0;            ///< Detection confidence 0.0-1.0
        double aspectRatio = 0.0;           ///< Measured width/height ratio
        bool held = false;                  ///< True when this is a held/stale result, not a fresh detection
    };

    /**
     * @brief Result of screen detection — optionally contains top and/or bottom screen.
     */
    struct ScreenDetectionResult
    {
        std::optional<DetectedScreen> topScreen;    ///< Detected top screen
        std::optional<DetectedScreen> bottomScreen; ///< Detected bottom screen
    };

    /**
     * @brief Configuration for the screen detector algorithm.
     */
    struct ScreenDetectorConfig
    {
        int brightnessThreshold = 80;                        ///< Minimum brightness for screen pixels (Otsu fallback)
        double minAreaFraction = 0.02;                       ///< Minimum contour area as fraction of frame area
        double maxAreaFraction = 0.5;                        ///< Maximum contour area as fraction of frame area
        double topAspectRatio = Core::kTopScreenAspectRatio; ///< Expected top screen aspect ratio (400/240)
        double bottomAspectRatio = Core::kBottomScreenAspectRatio; ///< Expected bottom screen aspect ratio (320/240)
        double aspectRatioTolerance = 0.25;                        ///< Allowable deviation from expected aspect ratio
        int smoothingWindowSize = 10;                              ///< EMA smoothing window size in frames
        int morphKernelSize = 5;                                   ///< Kernel size for morphological operations
        double polyEpsilonFraction = 0.02;                         ///< approxPolyDP epsilon as fraction of perimeter
        int calibrationFrames = 15; ///< Minimum frames in rolling window before calibration can lock
    };

    /**
     * @brief Automatically detects 3DS screens in a camera frame using brightness,
     *        contour analysis, and position-based classification.
     *
     * Detects screens during the first calibrationFrames frames, then locks in
     * the detected corners and returns cached results on subsequent calls.
     */
    class ScreenDetector
    {
    public:
        /** @brief Constructs a ScreenDetector with the given configuration. */
        explicit ScreenDetector(ScreenDetectorConfig config = {});

        /** @brief Detect screens in a single frame without temporal smoothing. */
        ScreenDetectionResult DetectOnce(const cv::Mat &cameraFrame) const;

        /** @brief Detect screens with temporal EMA smoothing. Locks after calibration. */
        ScreenDetectionResult Detect(const cv::Mat &cameraFrame);

        /** @brief Detect screens and apply corners to the given preprocessor. */
        void ApplyTo(FramePreprocessor &preprocessor, const cv::Mat &cameraFrame);

        /** @brief Whether calibration is complete (corners locked). */
        bool IsCalibrated() const;

        /** @brief Reset calibration and smoothing state. Forces re-detection. */
        void Reset();

        /** @brief Factory method following project conventions. */
        static std::unique_ptr<ScreenDetector> CreateScreenDetector(ScreenDetectorConfig config = {});

    private:
        /** @brief Find bright quadrilateral contours in the frame. */
        std::vector<DetectedScreen> FindCandidates(const cv::Mat &cameraFrame) const;

        /** @brief Classify candidates into top/bottom by vertical position and confidence. */
        ScreenDetectionResult ClassifyCandidates(std::vector<DetectedScreen> candidates) const;

        /** @brief Apply EMA smoothing to detected corners. */
        void SmoothCorners(ScreenDetectionResult &result);

        /** @brief Smooth a single screen's corners via EMA. */
        void SmoothSingleScreen(std::optional<DetectedScreen> &screen,
            std::optional<std::array<cv::Point2f, 4>> &smoothed,
            int &framesSinceDetection,
            double alpha);

        /** @brief Order contour points as TL, TR, BR, BL. */
        static std::array<cv::Point2f, 4> OrderCorners(const std::vector<cv::Point> &contour);

        /** @brief Validate that ordered corners form a sensible quadrilateral. */
        static bool ValidateCornerOrder(const std::array<cv::Point2f, 4> &corners);

        /** @brief Compute aspect ratio from ordered corners. */
        [[nodiscard]] static double ComputeAspectRatio(const std::array<cv::Point2f, 4> &corners);

        /** @brief Compute detection confidence from shape quality and aspect ratio match. */
        [[nodiscard]] double ComputeConfidence(const std::array<cv::Point2f, 4> &corners, double aspectRatio) const;

        ScreenDetectorConfig config; ///< Algorithm configuration

        // Temporal smoothing state
        std::optional<std::array<cv::Point2f, 4>> smoothedTopCorners;    ///< EMA-smoothed top corners
        std::optional<std::array<cv::Point2f, 4>> smoothedBottomCorners; ///< EMA-smoothed bottom corners
        int framesSinceTopDetection = 0;                                 ///< Frames since last top screen detection
        int framesSinceBottomDetection = 0;                              ///< Frames since last bottom screen detection

        // Position memory for single-screen classification
        float splitPointY = -1.0f; ///< Vertical midpoint between top/bottom screens (-1 = unknown)

        // Calibration state
        bool calibrated = false;                                    ///< Whether calibration is complete
        static constexpr int kCalibrationWindowSize = 20;           ///< Rolling window size
        static constexpr double kCalibrationSuccessThreshold = 0.8; ///< Required success rate (80%)
        std::deque<bool> calibrationWindow;                         ///< Rolling window of detection success/failure
        ScreenDetectionResult calibratedResult;                     ///< Locked result after calibration

        // Thread safety
        mutable std::mutex mutex; ///< Guards mutable state in Detect() and Reset()
    };

} // namespace SH3DS::Capture
