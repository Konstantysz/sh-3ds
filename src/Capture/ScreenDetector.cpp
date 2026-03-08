#include "ScreenDetector.h"

#include "FramePreprocessor.h"
#include "Kappa/Logger.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace SH3DS::Capture
{
    ScreenDetector::ScreenDetector(ScreenDetectorConfig config) : config(std::move(config))
    {
    }

    ScreenDetectionResult ScreenDetector::DetectOnce(const cv::Mat &cameraFrame) const
    {
        if (cameraFrame.empty())
        {
            return {};
        }

        auto candidates = FindCandidates(cameraFrame);
        return ClassifyCandidates(std::move(candidates));
    }

    ScreenDetectionResult ScreenDetector::Detect(const cv::Mat &cameraFrame)
    {
        std::lock_guard<std::mutex> lock(mutex);

        // Once calibrated, return locked result without re-detecting
        if (calibrated)
        {
            return calibratedResult;
        }

        auto result = DetectOnce(cameraFrame);
        SmoothCorners(result);

        // Update split point when both screens are visible
        if (result.topScreen && result.bottomScreen)
        {
            float topCenterY = (result.topScreen->corners[0].y + result.topScreen->corners[2].y) / 2.0f;
            float botCenterY = (result.bottomScreen->corners[0].y + result.bottomScreen->corners[2].y) / 2.0f;
            splitPointY = (topCenterY + botCenterY) / 2.0f;
        }

        // Rolling window calibration tracking
        bool bothDetected = result.topScreen.has_value() && result.bottomScreen.has_value();
        calibrationWindow.push_back(bothDetected);
        if (static_cast<int>(calibrationWindow.size()) > kCalibrationWindowSize)
        {
            calibrationWindow.pop_front();
        }

        // Calibrate when window has enough frames and success rate exceeds threshold
        if (static_cast<int>(calibrationWindow.size()) >= config.calibrationFrames)
        {
            int successes = static_cast<int>(std::count(calibrationWindow.begin(), calibrationWindow.end(), true));
            double successRate = static_cast<double>(successes) / static_cast<double>(calibrationWindow.size());

            if (successRate >= kCalibrationSuccessThreshold && bothDetected)
            {
                calibrated = true;
                calibratedResult = result;
                LOG_INFO("Screen detection calibrated ({:.0f}% success rate over {} frames)",
                    successRate * 100.0,
                    calibrationWindow.size());
                if (result.topScreen)
                {
                    LOG_INFO("  Top screen corners: [{:.0f},{:.0f}] [{:.0f},{:.0f}] [{:.0f},{:.0f}] [{:.0f},{:.0f}]",
                        result.topScreen->corners[0].x,
                        result.topScreen->corners[0].y,
                        result.topScreen->corners[1].x,
                        result.topScreen->corners[1].y,
                        result.topScreen->corners[2].x,
                        result.topScreen->corners[2].y,
                        result.topScreen->corners[3].x,
                        result.topScreen->corners[3].y);
                }
                if (result.bottomScreen)
                {
                    LOG_INFO("  Bottom screen corners: [{:.0f},{:.0f}] [{:.0f},{:.0f}] [{:.0f},{:.0f}] [{:.0f},{:.0f}]",
                        result.bottomScreen->corners[0].x,
                        result.bottomScreen->corners[0].y,
                        result.bottomScreen->corners[1].x,
                        result.bottomScreen->corners[1].y,
                        result.bottomScreen->corners[2].x,
                        result.bottomScreen->corners[2].y,
                        result.bottomScreen->corners[3].x,
                        result.bottomScreen->corners[3].y);
                }
            }
        }

        return result;
    }

    void ScreenDetector::ApplyTo(FramePreprocessor &preprocessor, const cv::Mat &cameraFrame)
    {
        auto detection = Detect(cameraFrame);
        if (detection.topScreen)
        {
            preprocessor.SetFixedCorners(detection.topScreen->corners);
        }
        if (detection.bottomScreen)
        {
            preprocessor.SetBottomCorners(detection.bottomScreen->corners);
        }
    }

    bool ScreenDetector::IsCalibrated() const
    {
        return calibrated;
    }

    void ScreenDetector::Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);

        smoothedTopCorners = std::nullopt;
        smoothedBottomCorners = std::nullopt;
        framesSinceTopDetection = 0;
        framesSinceBottomDetection = 0;
        splitPointY = -1.0f;
        calibrated = false;
        calibrationWindow.clear();
        calibratedResult = {};
    }

    std::unique_ptr<ScreenDetector> ScreenDetector::CreateScreenDetector(ScreenDetectorConfig config)
    {
        return std::make_unique<ScreenDetector>(std::move(config));
    }

    std::vector<DetectedScreen> ScreenDetector::FindCandidates(const cv::Mat &cameraFrame) const
    {
        std::vector<DetectedScreen> candidates;

        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(cameraFrame, gray, cv::COLOR_BGR2GRAY);

        // Gaussian blur to reduce noise
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

        // Adaptive thresholding: try Otsu first, fall back to configured threshold
        cv::Mat binary;
        double otsuThreshold = cv::threshold(blurred, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        if (otsuThreshold < config.brightnessThreshold)
        {
            cv::threshold(blurred, binary, config.brightnessThreshold, 255, cv::THRESH_BINARY);
        }

        // Morphological operations to clean up
        cv::Mat kernel =
            cv::getStructuringElement(cv::MORPH_RECT, cv::Size(config.morphKernelSize, config.morphKernelSize));
        cv::Mat cleaned;
        cv::morphologyEx(binary, cleaned, cv::MORPH_CLOSE, kernel);
        cv::morphologyEx(cleaned, cleaned, cv::MORPH_OPEN, kernel);

        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(cleaned, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        double frameArea = static_cast<double>(cameraFrame.cols) * cameraFrame.rows;
        double minArea = config.minAreaFraction * frameArea;
        double maxArea = config.maxAreaFraction * frameArea;

        for (const auto &contour : contours)
        {
            double area = cv::contourArea(contour);
            if (area < minArea || area > maxArea)
            {
                continue;
            }

            // Approximate polygon
            double epsilon = config.polyEpsilonFraction * cv::arcLength(contour, true);
            std::vector<cv::Point> approx;
            cv::approxPolyDP(contour, approx, epsilon, true);

            // Must be a quadrilateral and convex
            if (approx.size() != 4 || !cv::isContourConvex(approx))
            {
                continue;
            }

            auto corners = OrderCorners(approx);

            // Validate corner ordering (rejects heavily rotated quads)
            if (!ValidateCornerOrder(corners))
            {
                continue;
            }

            double aspectRatio = ComputeAspectRatio(corners);

            // Filter: must match either top or bottom expected aspect ratio
            double topDiff = std::abs(aspectRatio - config.topAspectRatio);
            double botDiff = std::abs(aspectRatio - config.bottomAspectRatio);
            if (topDiff > config.aspectRatioTolerance && botDiff > config.aspectRatioTolerance)
            {
                continue;
            }

            double confidence = ComputeConfidence(corners, aspectRatio);

            DetectedScreen screen;
            screen.corners = corners;
            screen.confidence = confidence;
            screen.aspectRatio = aspectRatio;
            candidates.push_back(screen);
        }

        if (candidates.empty() && !cameraFrame.empty())
        {
            LOG_DEBUG("ScreenDetector: No candidates found in non-empty frame (Otsu={:.0f}, fallback={})",
                otsuThreshold,
                config.brightnessThreshold);
        }

        return candidates;
    }

    ScreenDetectionResult ScreenDetector::ClassifyCandidates(std::vector<DetectedScreen> candidates) const
    {
        ScreenDetectionResult result;

        if (candidates.empty())
        {
            return result;
        }

        // When more than 2 candidates, select the 2 with highest confidence
        if (candidates.size() > 2)
        {
            LOG_WARN("ScreenDetector: {} candidates found, selecting top 2 by confidence", candidates.size());
            std::sort(candidates.begin(), candidates.end(), [](const DetectedScreen &a, const DetectedScreen &b) {
                return a.confidence > b.confidence;
            });
            candidates.resize(2);
        }

        // Sort remaining candidates by vertical center position (top of image first)
        std::sort(candidates.begin(), candidates.end(), [](const DetectedScreen &a, const DetectedScreen &b) {
            float aCenterY = (a.corners[0].y + a.corners[2].y) / 2.0f;
            float bCenterY = (b.corners[0].y + b.corners[2].y) / 2.0f;
            return aCenterY < bCenterY;
        });

        if (candidates.size() >= 2)
        {
            // Two screens — highest in image is top, next is bottom
            result.topScreen = candidates[0];
            result.bottomScreen = candidates[1];
        }
        else
        {
            // Single screen — hybrid classification using position + aspect ratio
            float centerY = (candidates[0].corners[0].y + candidates[0].corners[2].y) / 2.0f;
            double topDiff = std::abs(candidates[0].aspectRatio - config.topAspectRatio);
            double botDiff = std::abs(candidates[0].aspectRatio - config.bottomAspectRatio);

            // Score: positive = more likely top, negative = more likely bottom
            double score = 0.0;

            // Aspect ratio signal (weight 1.0)
            if (topDiff < botDiff)
            {
                score += 1.0;
            }
            else if (botDiff < topDiff)
            {
                score -= 1.0;
            }

            // Position signal when split point is known (weight 2.0 — stronger)
            if (splitPointY > 0.0f)
            {
                if (centerY < splitPointY)
                {
                    score += 2.0;
                }
                else
                {
                    score -= 2.0;
                }
            }

            if (score >= 0.0)
            {
                result.topScreen = candidates[0];
            }
            else
            {
                result.bottomScreen = candidates[0];
            }
        }

        return result;
    }

    void ScreenDetector::SmoothCorners(ScreenDetectionResult &result)
    {
        double alpha = 2.0 / (config.smoothingWindowSize + 1);

        SmoothSingleScreen(result.topScreen, smoothedTopCorners, framesSinceTopDetection, alpha);
        SmoothSingleScreen(result.bottomScreen, smoothedBottomCorners, framesSinceBottomDetection, alpha);
    }

    void ScreenDetector::SmoothSingleScreen(std::optional<DetectedScreen> &screen,
        std::optional<std::array<cv::Point2f, 4>> &smoothed,
        int &framesSinceDetection,
        double alpha)
    {
        if (screen)
        {
            framesSinceDetection = 0;
            if (smoothed)
            {
                const auto a = static_cast<float>(std::clamp(alpha, 0.0, 1.0));
                const auto b = 1.0f - a;
                for (size_t i = 0; i < 4; ++i)
                {
                    (*smoothed)[i].x = a * screen->corners[i].x + b * (*smoothed)[i].x;
                    (*smoothed)[i].y = a * screen->corners[i].y + b * (*smoothed)[i].y;
                }
            }
            else
            {
                smoothed = screen->corners;
            }
            screen->corners = *smoothed;
        }
        else
        {
            ++framesSinceDetection;
            if (framesSinceDetection > config.smoothingWindowSize)
            {
                smoothed = std::nullopt;
            }
            else if (smoothed)
            {
                DetectedScreen heldScreen;
                heldScreen.corners = *smoothed;
                heldScreen.confidence = 0.0;
                heldScreen.aspectRatio = ComputeAspectRatio(*smoothed);
                heldScreen.held = true;
                screen = heldScreen;
            }
        }
    }

    std::array<cv::Point2f, 4> ScreenDetector::OrderCorners(const std::vector<cv::Point> &contour)
    {
        std::array<cv::Point2f, 4> ordered;

        std::vector<cv::Point2f> pts;
        for (const auto &pt : contour)
        {
            pts.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }

        // TL: smallest x+y sum, TR: largest x-y, BR: largest x+y sum, BL: smallest x-y
        auto sumCompare = [](const cv::Point2f &a, const cv::Point2f &b) {
            return (a.x + a.y) < (b.x + b.y);
        };

        auto diffCompare = [](const cv::Point2f &a, const cv::Point2f &b) {
            return (a.x - a.y) < (b.x - b.y);
        };

        ordered[0] = *std::min_element(pts.begin(), pts.end(), sumCompare);  // TL
        ordered[1] = *std::max_element(pts.begin(), pts.end(), diffCompare); // TR
        ordered[2] = *std::max_element(pts.begin(), pts.end(), sumCompare);  // BR
        ordered[3] = *std::min_element(pts.begin(), pts.end(), diffCompare); // BL

        return ordered;
    }

    bool ScreenDetector::ValidateCornerOrder(const std::array<cv::Point2f, 4> &corners)
    {
        float cx = (corners[0].x + corners[1].x + corners[2].x + corners[3].x) / 4.0f;
        float cy = (corners[0].y + corners[1].y + corners[2].y + corners[3].y) / 4.0f;

        // TL should be left of and above center
        // TR should be right of and above center
        // BR should be right of and below center
        // BL should be left of and below center
        return corners[0].x < cx && corners[0].y < cy && corners[1].x > cx && corners[1].y < cy && corners[2].x > cx
               && corners[2].y > cy && corners[3].x < cx && corners[3].y > cy;
    }

    double ScreenDetector::ComputeAspectRatio(const std::array<cv::Point2f, 4> &corners)
    {
        double topWidth = cv::norm(corners[1] - corners[0]);
        double bottomWidth = cv::norm(corners[2] - corners[3]);
        double avgWidth = (topWidth + bottomWidth) / 2.0;

        double leftHeight = cv::norm(corners[3] - corners[0]);
        double rightHeight = cv::norm(corners[2] - corners[1]);
        double avgHeight = (leftHeight + rightHeight) / 2.0;

        if (avgHeight < 1.0)
        {
            return 0.0;
        }
        return avgWidth / avgHeight;
    }

    double ScreenDetector::ComputeConfidence(const std::array<cv::Point2f, 4> &corners, double aspectRatio) const
    {
        const double topDiff = std::abs(aspectRatio - config.topAspectRatio);
        const double botDiff = std::abs(aspectRatio - config.bottomAspectRatio);
        const double bestDiff = std::min(topDiff, botDiff);

        double confidence = 1.0 - (bestDiff / (config.aspectRatioTolerance * 2.0));
        confidence = std::clamp(confidence, 0.0, 1.0);

        const double topWidth = cv::norm(corners[1] - corners[0]);
        const double bottomWidth = cv::norm(corners[2] - corners[3]);
        const double leftHeight = cv::norm(corners[3] - corners[0]);
        const double rightHeight = cv::norm(corners[2] - corners[1]);

        const double maxWidth = std::max(topWidth, bottomWidth);
        const double maxHeight = std::max(leftHeight, rightHeight);
        if (maxWidth < 1.0 || maxHeight < 1.0)
        {
            return 0.0;
        }
        const double widthRatio = std::min(topWidth, bottomWidth) / maxWidth;
        const double heightRatio = std::min(leftHeight, rightHeight) / maxHeight;

        confidence *= (widthRatio + heightRatio) / 2.0;

        return confidence;
    }

} // namespace SH3DS::Capture
