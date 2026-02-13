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
         * @brief Processes a camera frame and extracts ROIs.
         * @param cameraFrame The camera frame to process.
         * @return An optional containing the ROISet if successful, an empty optional otherwise.
         */
        std::optional<Core::ROISet> Process(const cv::Mat &cameraFrame) const;

        /**
         * @brief Sets the fixed corners for the screen.
         * @param corners The fixed corners.
         */
        void SetFixedCorners(std::array<cv::Point2f, 4> corners);

    private:
        /**
         * @brief Recalculates the warp matrix.
         */
        void RecalculateWarpMatrix();

        Core::ScreenCalibrationConfig calibration; ///< The screen calibration configuration.
        std::vector<Core::RoiDefinition> roiDefs;  ///< The ROI definitions.
        cv::Mat warpMatrix;                        ///< The warp matrix.
    };
} // namespace SH3DS::Capture
