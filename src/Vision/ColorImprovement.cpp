#include "Vision/ColorImprovement.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    /**
     * @brief Build a gamma look-up table for uint8 values.
     * @param gamma The gamma value to use.
     * @return A look-up table for gamma correction.
     */
    cv::Mat BuildGammaLut(double gamma)
    {
        cv::Mat lut(1, 256, CV_8U);
        auto *p = lut.ptr<uchar>();
        for (int i = 0; i < 256; ++i)
        {
            double normalised = static_cast<double>(i) / 255.0;
            double corrected = std::pow(normalised, gamma) * 255.0;
            p[i] = static_cast<uchar>(std::clamp(corrected, 0.0, 255.0));
        }
        return lut;
    }
} // namespace

namespace SH3DS::Vision
{
    cv::Mat ImproveFrameColors(const cv::Mat &frame, const ColorImprovementConfig &config)
    {
        if (frame.empty() || frame.type() != CV_8UC3)
        {
            return frame;
        }

        // Cache CLAHE and gamma LUT — rebuilt only when config changes.
        static ColorImprovementConfig cachedConfig{};
        static cv::Ptr<cv::CLAHE> cachedClahe = cv::createCLAHE(
            cachedConfig.claheClipLimit, cv::Size(cachedConfig.claheTileWidth, cachedConfig.claheTileHeight));
        static cv::Mat cachedGammaLut = BuildGammaLut(cachedConfig.gamma);

        if (config.claheClipLimit != cachedConfig.claheClipLimit || config.claheTileWidth != cachedConfig.claheTileWidth
            || config.claheTileHeight != cachedConfig.claheTileHeight || config.gamma != cachedConfig.gamma)
        {
            cachedClahe =
                cv::createCLAHE(config.claheClipLimit, cv::Size(config.claheTileWidth, config.claheTileHeight));
            cachedGammaLut = BuildGammaLut(config.gamma);
            cachedConfig = config;
        }

        cv::Mat working;

        // ------------------------------------------------------------------
        // Stage 1: Gray World white balance
        // ------------------------------------------------------------------
        cv::Mat floatFrame;
        frame.convertTo(floatFrame, CV_32FC3, 1.0 / 255.0);

        cv::Scalar means = cv::mean(floatFrame);
        double grayMean = (means[0] + means[1] + means[2]) / 3.0;

        std::array<cv::Mat, 3> channels;
        cv::split(floatFrame, channels.data());
        for (std::size_t c = 0; c < 3; ++c)
        {
            double channelMean = means[static_cast<int>(c)];
            double safeMean = std::max(channelMean, 1e-4);
            double gain = std::clamp(grayMean / safeMean, config.wbGainMin, config.wbGainMax);
            channels[c].convertTo(channels[c], -1, gain);

            cv::threshold(channels[c], channels[c], 1.0, 1.0, cv::THRESH_TRUNC);
        }
        cv::merge(channels.data(), 3, floatFrame);

        floatFrame.convertTo(working, CV_8UC3, 255.0);

        // ------------------------------------------------------------------
        // Stage 2: CLAHE on the L channel (LAB space)
        // ------------------------------------------------------------------
        cv::Mat lab;
        cv::cvtColor(working, lab, cv::COLOR_BGR2Lab);

        std::array<cv::Mat, 3> labChannels;
        cv::split(lab, labChannels.data());

        cachedClahe->apply(labChannels[0], labChannels[0]);

        cv::merge(labChannels.data(), 3, lab);
        cv::cvtColor(lab, working, cv::COLOR_Lab2BGR);

        // ------------------------------------------------------------------
        // Stage 3: Gamma correction via LUT
        // ------------------------------------------------------------------
        cv::Mat outputFrame;
        cv::LUT(working, cachedGammaLut, outputFrame);

        return outputFrame;
    }
} // namespace SH3DS::Vision
