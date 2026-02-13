#pragma once

#include <opencv2/core.hpp>

#include <map>
#include <string>

namespace SH3DS::Vision
{
    /**
     * @brief Utility for template matching with caching.
     */
    class TemplateMatcher
    {
    public:
        /**
         * @brief Match a template against a region.
         * @param region The image region to match against.
         * @param templatePath Path to the template image file.
         * @return Confidence score in [0.0, 1.0].
         */
        double Match(const cv::Mat &region, const std::string &templatePath);

    private:
        std::map<std::string, cv::Mat> cache; ///< Loaded template cache
    };
} // namespace SH3DS::Vision
