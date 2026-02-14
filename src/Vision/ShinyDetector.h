#pragma once

#include "Core/Config.h"
#include "Core/Types.h"

#include <opencv2/core.hpp>

#include <memory>
#include <span>
#include <string>

namespace SH3DS::Vision
{
    /**
     * @brief Abstract base class for shiny Pokemon detection.
     */
    class ShinyDetector
    {
    public:
        virtual ~ShinyDetector() = default;

        /**
         * @brief Detect shiny Pokemon in an image.
         * @param pokemonRoi The image region containing the Pokemon.
         * @return The shiny detection result.
         */
        virtual Core::ShinyResult Detect(const cv::Mat &pokemonRoi) const = 0;

        /**
         * @brief Detect shiny Pokemon in a sequence of images.
         * @param rois The sequence of image regions containing the Pokemon.
         * @return The shiny detection result.
         */
        virtual Core::ShinyResult DetectSequence(std::span<const cv::Mat> rois) const = 0;

        /**
         * @brief Get the profile ID.
         * @return The profile ID.
         */
        virtual std::string ProfileId() const = 0;

        /**
         * @brief Reset the detector.
         */
        virtual void Reset() = 0;
    };
} // namespace SH3DS::Vision
