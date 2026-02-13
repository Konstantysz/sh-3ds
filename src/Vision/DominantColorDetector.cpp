#include "DominantColorDetector.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>

namespace SH3DS::Vision
{
    DominantColorDetector::DominantColorDetector(Core::DetectionMethodConfig config, std::string profileId)
        : config(std::move(config)),
          id(std::move(profileId))
    {
    }

    Core::ShinyResult DominantColorDetector::Detect(const cv::Mat &pokemonRoi)
    {
        if (pokemonRoi.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain, .confidence = 0.0, .method = "dominant_color" };
        }

        cv::Mat hsv;
        cv::cvtColor(pokemonRoi, hsv, cv::COLOR_BGR2HSV);

        cv::Mat normalMask;
        cv::inRange(hsv, config.normalHsvLower, config.normalHsvUpper, normalMask);
        double normalRatio = cv::countNonZero(normalMask) / static_cast<double>(pokemonRoi.total());

        cv::Mat shinyMask;
        cv::inRange(hsv, config.shinyHsvLower, config.shinyHsvUpper, shinyMask);
        double shinyRatio = cv::countNonZero(shinyMask) / static_cast<double>(pokemonRoi.total());

        std::string details = "normal=" + std::to_string(normalRatio) + " shiny=" + std::to_string(shinyRatio);

        if (shinyRatio >= config.shinyRatioThreshold && shinyRatio > normalRatio)
        {
            return {
                .verdict = Core::ShinyVerdict::Shiny,
                .confidence = std::min(shinyRatio / config.shinyRatioThreshold, 1.0),
                .method = "dominant_color",
                .details = details,
            };
        }

        if (normalRatio >= config.normalRatioThreshold)
        {
            return {
                .verdict = Core::ShinyVerdict::NotShiny,
                .confidence = std::min(normalRatio / config.normalRatioThreshold, 1.0),
                .method = "dominant_color",
                .details = details,
            };
        }

        return {
            .verdict = Core::ShinyVerdict::Uncertain,
            .confidence = 0.0,
            .method = "dominant_color",
            .details = details,
        };
    }

    Core::ShinyResult DominantColorDetector::DetectSequence(std::span<const cv::Mat> rois)
    {
        if (rois.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain, .confidence = 0.0, .method = "dominant_color" };
        }
        // Use the middle frame for single-shot analysis
        return Detect(rois[rois.size() / 2]);
    }

    std::string DominantColorDetector::ProfileId() const
    {
        return id;
    }

    void DominantColorDetector::Reset()
    {
        // No internal state to reset for single-frame detection
    }

    std::unique_ptr<ShinyDetector> DominantColorDetector::CreateDominantColorDetector(
        const Core::DetectionMethodConfig &config,
        const std::string &profileId)
    {
        return std::make_unique<DominantColorDetector>(config, profileId);
    }
} // namespace SH3DS::Vision
