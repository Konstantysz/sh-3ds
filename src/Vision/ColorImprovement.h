#pragma once

#include <opencv2/core.hpp>

namespace SH3DS::Vision
{
    /**
     * @brief Configuration for the post-warp frame color correction pipeline.
     */
    struct ColorImprovementConfig
    {
        double wbGainMin = 0.5;      ///< Minimum per-channel gain clamp
        double wbGainMax = 3.0;      ///< Maximum per-channel gain clamp
        double claheClipLimit = 4.0; ///< CLAHE clip limit
        int claheTileWidth = 6;      ///< CLAHE tile grid width
        int claheTileHeight = 6;     ///< CLAHE tile grid height
        double gamma = 1.1;          ///< Gamma value (> 1.0 darkens midtones/shadows)
    };

    /**
     * @brief Applies a three-stage color correction to a warped screen frame.
     *
     * Runs Gray World white balance, CLAHE contrast enhancement, and gamma
     * correction and returns the corrected result as a new image. The input
     * must be a non-empty 8-bit 3-channel BGR image; other formats are returned
     * unchanged.
     *
     * The CLAHE object and gamma LUT are cached and only rebuilt when @p config
     * differs from the previous call.
     *
     * @param frame  BGR image to correct.
     * @param config Pipeline parameters (defaults give sensible results).
     * @return Corrected BGR image.
     */
    [[nodiscard]] cv::Mat ImproveFrameColors(const cv::Mat &frame, const ColorImprovementConfig &config = {});
} // namespace SH3DS::Vision
