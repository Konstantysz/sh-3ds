#pragma once

#include "Core/Config.h"
#include "Core/Types.h"

#include <opencv2/core.hpp>

#include <array>
#include <optional>
#include <vector>

namespace SH3DS::Capture
{
    /**
     * @brief Result of dual-screen perspective warp.
     */
    struct DualScreenResult
    {
        cv::Mat warpedTop;                   ///< Full warped top screen image
        cv::Mat warpedBottom;                ///< Full warped bottom screen image (empty if no calibration)
        std::optional<Core::ROISet> topRois; ///< Extracted ROIs from top screen
    };

    /**
     * @brief Locates the 3DS screen in a camera frame, corrects perspective, and extracts named ROIs.
     */
    class FramePreprocessor
    {
    public:
        /**
         * @brief Constructs a new FramePreprocessor.
         * @param calibration The screen calibration configuration.
         * @param roiDefs The ROI definitions.
         */
        explicit FramePreprocessor(Core::ScreenCalibrationConfig calibration, std::vector<Core::RoiDefinition> roiDefs);

        /**
         * @brief Constructs a new FramePreprocessor with optional bottom screen calibration.
         * @param calibration The top screen calibration configuration.
         * @param roiDefs The ROI definitions (applied to top screen).
         * @param bottomCalibration The bottom screen calibration (optional).
         */
        FramePreprocessor(Core::ScreenCalibrationConfig calibration,
            std::vector<Core::RoiDefinition> roiDefs,
            std::optional<Core::ScreenCalibrationConfig> bottomCalibration);

        /**
         * @brief Processes a camera frame and extracts ROIs.
         * @param cameraFrame The camera frame to process.
         * @return An optional containing the ROISet if successful, an empty optional otherwise.
         */
        std::optional<Core::ROISet> Process(const cv::Mat &cameraFrame) const;

        /**
         * @brief Processes both top and bottom screens from a single camera frame.
         * @param cameraFrame The raw camera frame.
         * @return Dual screen result if successful, nullopt otherwise.
         */
        std::optional<DualScreenResult> ProcessDualScreen(const cv::Mat &cameraFrame) const;

        /**
         * @brief Sets the fixed corners for the top screen.
         * @param corners The fixed corners.
         */
        void SetFixedCorners(std::array<cv::Point2f, 4> corners);

        /**
         * @brief Sets the fixed corners for the bottom screen.
         * @param corners The fixed corners.
         */
        void SetBottomCorners(std::array<cv::Point2f, 4> corners);

    private:
        /**
         * @brief Extracts named ROIs from a warped screen image.
         * @param warpedImage The perspective-corrected screen image.
         * @param calib The calibration config used for coordinate mapping.
         * @return Map of ROI name to extracted sub-image.
         */
        Core::ROISet ExtractRois(const cv::Mat &warpedImage, const Core::ScreenCalibrationConfig &calib) const;

        /**
         * @brief Recalculates the warp matrix.
         */
        void RecalculateWarpMatrix();

        /**
         * @brief Calculates a warp matrix from calibration config.
         * @param calib The calibration config.
         * @return The perspective transform matrix.
         */
        static cv::Mat CalculateWarpMatrix(const Core::ScreenCalibrationConfig &calib);

        Core::ScreenCalibrationConfig calibration;                      ///< Top screen calibration
        std::vector<Core::RoiDefinition> roiDefs;                       ///< The ROI definitions
        cv::Mat warpMatrix;                                             ///< Top screen warp matrix
        std::optional<Core::ScreenCalibrationConfig> bottomCalibration; ///< Bottom screen calibration
        cv::Mat bottomWarpMatrix;                                       ///< Bottom screen warp matrix
    };
} // namespace SH3DS::Capture
