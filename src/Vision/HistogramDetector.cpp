#include "HistogramDetector.h"

#include "Core/Logging.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>

namespace SH3DS::Vision
{
    HistogramDetector::HistogramDetector(Core::DetectionMethodConfig config, std::string profileId)
        : config(std::move(config)),
          id(std::move(profileId))
    {
    }

    Core::ShinyResult HistogramDetector::Detect(const cv::Mat &pokemonRoi)
    {
        if (pokemonRoi.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain, .confidence = 0.0, .method = "histogram_compare" };
        }

        cv::Mat hsv;
        cv::cvtColor(pokemonRoi, hsv, cv::COLOR_BGR2HSV);

        // Compute HS histogram of the ROI
        cv::Mat roiHist;
        int channels[] = { 0, 1 };
        int histSize[] = { 30, 32 }; // H: 30 bins, S: 32 bins
        float hRange[] = { 0, 180 };
        float sRange[] = { 0, 256 };
        const float *ranges[] = { hRange, sRange };
        cv::calcHist(&hsv, 1, channels, cv::Mat(), roiHist, 2, histSize, ranges);
        cv::normalize(roiHist, roiHist, 0, 1, cv::NORM_MINMAX);

        // Load references lazily
        LoadReferences();

        if (normalHist.empty() || shinyHist.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain,
                .confidence = 0.0,
                .method = "histogram_compare",
                .details = "missing reference histograms" };
        }

        int method = cv::HISTCMP_CORREL;
        if (config.compareMethod == "chi_square")
        {
            method = cv::HISTCMP_CHISQR;
        }
        else if (config.compareMethod == "intersection")
        {
            method = cv::HISTCMP_INTERSECT;
        }
        else if (config.compareMethod == "bhattacharyya")
        {
            method = cv::HISTCMP_BHATTACHARYYA;
        }

        double normalCorr = cv::compareHist(roiHist, normalHist, method);
        double shinyCorr = cv::compareHist(roiHist, shinyHist, method);

        // Distance metrics (lower is better) need to be inverted for the differential calculation
        if (method == cv::HISTCMP_CHISQR || method == cv::HISTCMP_BHATTACHARYYA)
        {
            normalCorr = -normalCorr;
            shinyCorr = -shinyCorr;
        }

        std::string details = "normal_corr=" + std::to_string(normalCorr) + " shiny_corr=" + std::to_string(shinyCorr);

        double differential = shinyCorr - normalCorr;

        if (differential > config.differentialThreshold)
        {
            return {
                .verdict = Core::ShinyVerdict::Shiny,
                .confidence = std::min(differential / config.differentialThreshold, 1.0),
                .method = "histogram_compare",
                .details = details,
            };
        }

        if (differential < -config.differentialThreshold)
        {
            return {
                .verdict = Core::ShinyVerdict::NotShiny,
                .confidence = std::min(-differential / config.differentialThreshold, 1.0),
                .method = "histogram_compare",
                .details = details,
            };
        }

        return {
            .verdict = Core::ShinyVerdict::Uncertain,
            .confidence = 0.0,
            .method = "histogram_compare",
            .details = details,
        };
    }

    Core::ShinyResult HistogramDetector::DetectSequence(std::span<const cv::Mat> rois)
    {
        if (rois.empty())
        {
            return { .verdict = Core::ShinyVerdict::Uncertain, .confidence = 0.0, .method = "histogram_compare" };
        }
        return Detect(rois[rois.size() / 2]);
    }

    std::string HistogramDetector::ProfileId() const
    {
        return id;
    }

    void HistogramDetector::Reset()
    {
        // No internal state to reset
    }

    void HistogramDetector::LoadReferences()
    {
        if (referencesLoaded)
        {
            return;
        }
        referencesLoaded = true;

        if (!config.referenceNormal.empty())
        {
            cv::FileStorage fs(config.referenceNormal, cv::FileStorage::READ);
            if (fs.isOpened())
            {
                fs["histogram"] >> normalHist;
            }
            else
            {
                LOG_WARN("Failed to load normal histogram: {}", config.referenceNormal);
            }
        }

        if (!config.referenceShiny.empty())
        {
            cv::FileStorage fs(config.referenceShiny, cv::FileStorage::READ);
            if (fs.isOpened())
            {
                fs["histogram"] >> shinyHist;
            }
            else
            {
                LOG_WARN("Failed to load shiny histogram: {}", config.referenceShiny);
            }
        }
    }

    std::unique_ptr<ShinyDetector> HistogramDetector::CreateHistogramDetector(const Core::DetectionMethodConfig &config,
        const std::string &profileId)
    {
        return std::make_unique<HistogramDetector>(config, profileId);
    }

} // namespace SH3DS::Vision
