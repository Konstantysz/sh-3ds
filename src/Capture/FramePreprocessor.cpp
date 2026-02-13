#include "FramePreprocessor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace SH3DS::Capture
{
    FramePreprocessor::FramePreprocessor(Core::ScreenCalibrationConfig calibration,
                                         std::vector<Core::RoiDefinition> roiDefs)
        : calibration(std::move(calibration)),
          roiDefs(std::move(roiDefs))
    {
        RecalculateWarpMatrix();
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

        Core::ROISet result;
        for (const auto &roiDef : roiDefs)
        {
            int x = static_cast<int>(roiDef.x * calibration.targetWidth);
            int y = static_cast<int>(roiDef.y * calibration.targetHeight);
            int w = static_cast<int>(roiDef.w * calibration.targetWidth);
            int h = static_cast<int>(roiDef.h * calibration.targetHeight);

            x = std::max(0, std::min(x, calibration.targetWidth - 1));
            y = std::max(0, std::min(y, calibration.targetHeight - 1));
            w = std::min(w, calibration.targetWidth - x);
            h = std::min(h, calibration.targetHeight - y);

            if (w > 0 && h > 0)
            {
                result[roiDef.name] = warped(cv::Rect(x, y, w, h)).clone();
            }
        }

        return result;
    }

    void FramePreprocessor::SetFixedCorners(std::array<cv::Point2f, 4> corners)
    {
        calibration.corners = corners;
        RecalculateWarpMatrix();
    }

    void FramePreprocessor::RecalculateWarpMatrix()
    {
        std::vector<cv::Point2f> dst = {
            cv::Point2f(0.0f, 0.0f),
            cv::Point2f(static_cast<float>(calibration.targetWidth), 0.0f),
            cv::Point2f(static_cast<float>(calibration.targetWidth), static_cast<float>(calibration.targetHeight)),
            cv::Point2f(0.0f, static_cast<float>(calibration.targetHeight)),
        };

        std::vector<cv::Point2f> src(calibration.corners.begin(), calibration.corners.end());
        warpMatrix = cv::getPerspectiveTransform(src, dst);
    }

} // namespace SH3DS::Capture
