#pragma once

#include "ShinyDetector.h"

#include <span>
#include <string>

namespace SH3DS::Vision
{
    /**
     * @brief Detects shiny Pokemon by comparing dominant color ratios in the sprite ROI.
     */
    class DominantColorDetector : public ShinyDetector
    {
    public:
        /**
         * @brief Constructs a DominantColorDetector.
         * @param config Detection method configuration with HSV bounds and ratio thresholds.
         * @param profileId Profile identifier for this detector instance.
         */
        explicit DominantColorDetector(Core::DetectionMethodConfig config, std::string profileId);

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
         * @brief Resets internal state (no-op for this stateless detector).
         */
        void Reset() override;

        /**
         * @brief Create a dominant color detector.
         * @param config The detection method configuration.
         * @param profileId The profile ID.
         * @return A unique pointer to the dominant color detector.
         */
        static std::unique_ptr<ShinyDetector> CreateDominantColorDetector(const Core::DetectionMethodConfig &config,
            const std::string &profileId);

    private:
        Core::DetectionMethodConfig config; ///< Detection method configuration
        std::string id;                     ///< Profile identifier
    };
} // namespace SH3DS::Vision
