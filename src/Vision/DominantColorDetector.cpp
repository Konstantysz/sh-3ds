#include "DominantColorDetector.h"

#include "Kappa/Logger.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace
{
    /**
     * @brief Swaps lower/upper bounds if inverted and warns about Hue overflow.
     * @param lower Lower bound HSV values.
     * @param upper Upper bound HSV values.
     * @param name Name of the detection method.
     */
    void ValidateHsvRange(cv::Scalar &lower, cv::Scalar &upper, const std::string &name)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (lower[i] > upper[i])
            {
                std::swap(lower[i], upper[i]);
            }
        }
        if (upper[0] > 179)
        {
            LOG_WARN("DetectionProfile: Hue upper bound ({:.0f}) > 179 for method {}. OpenCV Hue range is 0-179.",
                static_cast<double>(upper[0]),
                name);
        }
    }
} // namespace

namespace SH3DS::Vision
{
    DominantColorDetector::DominantColorDetector(Core::DetectionMethodConfig config, std::string profileId)
        : config(std::move(config)),
          id(std::move(profileId))
    {
        ValidateHsvRange(this->config.normalHsvLower, this->config.normalHsvUpper, "normal");
        ValidateHsvRange(this->config.shinyHsvLower, this->config.shinyHsvUpper, "shiny");
    }

    Core::ShinyResult DominantColorDetector::Detect(const cv::Mat &pokemonRoi) const
    {
        if (pokemonRoi.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain,
                .confidence = 0.0,
                .method = "dominant_color",
                .details = {},
                .debugImage = {} };
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
                .debugImage = {},
            };
        }

        if (normalRatio >= config.normalRatioThreshold)
        {
            return {
                .verdict = Core::ShinyVerdict::NotShiny,
                .confidence = std::min(normalRatio / config.normalRatioThreshold, 1.0),
                .method = "dominant_color",
                .details = details,
                .debugImage = {},
            };
        }

        return {
            .verdict = Core::ShinyVerdict::Uncertain,
            .confidence = 0.0,
            .method = "dominant_color",
            .details = details,
            .debugImage = {},
        };
    }

    Core::ShinyResult DominantColorDetector::DetectSequence(std::span<const cv::Mat> rois) const
    {
        if (rois.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain,
                .confidence = 0.0,
                .method = "dominant_color",
                .details = {},
                .debugImage = {} };
        }

        std::map<Core::ShinyVerdict, int> votes;
        std::map<Core::ShinyVerdict, double> totalConfidence;

        for (const auto &roi : rois)
        {
            auto res = Detect(roi);
            votes[res.verdict]++;
            totalConfidence[res.verdict] += res.confidence;
        }

        // Find the verdict with the most votes
        Core::ShinyVerdict winner = Core::ShinyVerdict::Uncertain;
        int maxVotes = -1;

        for (const auto &[verdict, count] : votes)
        {
            if (count > maxVotes)
            {
                maxVotes = count;
                winner = verdict;
            }
        }

        return { .verdict = winner,
            .confidence = totalConfidence[winner] / static_cast<double>(maxVotes),
            .method = "dominant_color",
            .details = "sequence_majority_vote: count=" + std::to_string(maxVotes) + "/" + std::to_string(rois.size()),
            .debugImage = {} };
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
