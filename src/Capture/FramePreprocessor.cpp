#include "FramePreprocessor.h"

#include "Kappa/Logger.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace SH3DS::Capture
{
    FramePreprocessor::FramePreprocessor(Core::ScreenCalibrationConfig calibration,
        std::vector<Core::RoiDefinition> roiDefs)
        : calibration(std::move(calibration)),
          roiDefs(std::move(roiDefs))
    {
        RecalculateWarpMatrix();
    }

    FramePreprocessor::FramePreprocessor(Core::ScreenCalibrationConfig calibration,
        std::vector<Core::RoiDefinition> roiDefs,
        std::optional<Core::ScreenCalibrationConfig> bottomCalibration)
        : calibration(std::move(calibration)),
          roiDefs(std::move(roiDefs)),
          bottomCalibration(std::move(bottomCalibration))
    {
        RecalculateWarpMatrix();
        if (this->bottomCalibration)
        {
            auto &c = this->bottomCalibration->corners;
            bool degenerate = (c[0] == c[1] && c[1] == c[2] && c[2] == c[3]);
            if (degenerate)
            {
                LOG_WARN("Bottom screen calibration has degenerate corners — disabling");
                this->bottomCalibration = std::nullopt;
            }
            else
            {
                bottomWarpMatrix = CalculateWarpMatrix(*this->bottomCalibration);
            }
        }
    }

    std::optional<Core::ROISet> FramePreprocessor::Process(const cv::Mat &cameraFrame) const
    {
        if (cameraFrame.empty() || warpMatrix.empty())
        {
            return std::nullopt;
        }

        cv::Mat warped;
        cv::warpPerspective(
            cameraFrame, warped, warpMatrix, cv::Size(calibration.targetWidth, calibration.targetHeight));

        return ExtractRois(warped, calibration);
    }

    std::optional<DualScreenResult> FramePreprocessor::ProcessDualScreen(const cv::Mat &cameraFrame) const
    {
        if (cameraFrame.empty() || warpMatrix.empty())
        {
            return std::nullopt;
        }

        DualScreenResult result;

        // warpedTop/warpedBottom are independent allocations from warpPerspective.
        // ROIs are cloned in ExtractRois because sub-regions share memory with the source Mat.
        cv::warpPerspective(
            cameraFrame, result.warpedTop, warpMatrix, cv::Size(calibration.targetWidth, calibration.targetHeight));

        // Extract ROIs from top screen
        result.topRois = ExtractRois(result.warpedTop, calibration);

        // Warp bottom screen if calibrated
        if (bottomCalibration && !bottomWarpMatrix.empty())
        {
            cv::warpPerspective(cameraFrame,
                result.warpedBottom,
                bottomWarpMatrix,
                cv::Size(bottomCalibration->targetWidth, bottomCalibration->targetHeight));
        }

        return result;
    }

    Core::ROISet FramePreprocessor::ExtractRois(const cv::Mat &warpedImage,
        const Core::ScreenCalibrationConfig &calib) const
    {
        Core::ROISet rois;
        for (const auto &roiDef : roiDefs)
        {
            int x = static_cast<int>(std::round(roiDef.x * calib.targetWidth));
            int y = static_cast<int>(std::round(roiDef.y * calib.targetHeight));
            int w = static_cast<int>(std::round(roiDef.w * calib.targetWidth));
            int h = static_cast<int>(std::round(roiDef.h * calib.targetHeight));

            x = std::max(0, std::min(x, calib.targetWidth - 1));
            y = std::max(0, std::min(y, calib.targetHeight - 1));
            w = std::min(w, calib.targetWidth - x);
            h = std::min(h, calib.targetHeight - y);

            if (w > 0 && h > 0)
            {
                // Clone to give caller independent ownership (sub-regions share memory with source)
                rois[roiDef.name] = warpedImage(cv::Rect(x, y, w, h)).clone();
            }
        }
        return rois;
    }

    void FramePreprocessor::SetFixedCorners(std::array<cv::Point2f, 4> corners)
    {
        calibration.corners = corners;
        RecalculateWarpMatrix();
    }

    void FramePreprocessor::RecalculateWarpMatrix()
    {
        warpMatrix = CalculateWarpMatrix(calibration);
    }

    cv::Mat FramePreprocessor::CalculateWarpMatrix(const Core::ScreenCalibrationConfig &calib)
    {
        std::vector<cv::Point2f> dst = {
            cv::Point2f(0.0f, 0.0f),
            cv::Point2f(static_cast<float>(calib.targetWidth), 0.0f),
            cv::Point2f(static_cast<float>(calib.targetWidth), static_cast<float>(calib.targetHeight)),
            cv::Point2f(0.0f, static_cast<float>(calib.targetHeight)),
        };

        std::vector<cv::Point2f> src(calib.corners.begin(), calib.corners.end());
        return cv::getPerspectiveTransform(src, dst);
    }

} // namespace SH3DS::Capture
