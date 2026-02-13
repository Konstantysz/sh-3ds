#pragma once

#include "ShinyDetector.h"

#include <opencv2/core.hpp>

#include <span>
#include <string>

namespace SH3DS::Vision
{
    /**
     * @brief Detects shiny Pokemon by comparing HS histograms against pre-captured references.
     */
    class HistogramDetector : public ShinyDetector
    {
    public:
        /**
         * @brief Constructs a HistogramDetector.
         * @param config Detection method configuration with reference paths and comparison settings.
         * @param profileId Profile identifier for this detector instance.
         */
        explicit HistogramDetector(Core::DetectionMethodConfig config, std::string profileId);

        /**
         * @brief Detects shiny status from a single ROI frame.
         */
        Core::ShinyResult Detect(const cv::Mat &pokemonRoi) override;

        /**
         * @brief Detects shiny status from a sequence of ROI frames (uses middle frame).
         */
        Core::ShinyResult DetectSequence(std::span<const cv::Mat> rois) override;

        /**
         * @brief Returns the profile identifier.
         */
        std::string ProfileId() const override;

        /**
         * @brief Resets internal state (no-op for this detector).
         */
        void Reset() override;

        /**
         * @brief Create a histogram detector.
         * @param config The detection method configuration.
         * @param profileId The profile ID.
         * @return A unique pointer to the histogram detector.
         */
        static std::unique_ptr<ShinyDetector> CreateHistogramDetector(const Core::DetectionMethodConfig &config,
            const std::string &profileId);

    private:
        /**
         * @brief Lazily loads reference histograms from configured file paths.
         */
        void LoadReferences();

        Core::DetectionMethodConfig config; ///< Detection method configuration
        std::string id;                     ///< Profile identifier
        cv::Mat normalHist;                 ///< Reference histogram for normal appearance
        cv::Mat shinyHist;                  ///< Reference histogram for shiny appearance
        bool referencesLoaded = false;      ///< Whether references have been loaded
    };

} // namespace SH3DS::Vision
