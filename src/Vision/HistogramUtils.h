#pragma once

#include <opencv2/core.hpp>

#include <string>

namespace SH3DS::Vision
{
    /**
     * @brief Compute a 2D Hue-Saturation histogram from a BGR image.
     */
    cv::Mat ComputeHSHistogram(const cv::Mat &bgrImage, int hBins, int sBins);

    /**
     * @brief Save a histogram to a YAML file via cv::FileStorage.
     */
    void SaveHistogram(const cv::Mat &hist, const std::string &path);

    /**
     * @brief Load a histogram from a YAML file via cv::FileStorage.
     */
    cv::Mat LoadHistogram(const std::string &path);
} // namespace SH3DS::Vision
