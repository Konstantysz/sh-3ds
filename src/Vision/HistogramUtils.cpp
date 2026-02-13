#include "HistogramUtils.h"

#include <opencv2/imgproc.hpp>

namespace SH3DS::Vision
{

    cv::Mat ComputeHSHistogram(const cv::Mat &bgrImage, int hBins, int sBins)
    {
        cv::Mat hsv;
        cv::cvtColor(bgrImage, hsv, cv::COLOR_BGR2HSV);

        cv::Mat hist;
        int channels[] = { 0, 1 };
        int histSize[] = { hBins, sBins };
        float hRange[] = { 0, 180 };
        float sRange[] = { 0, 256 };
        const float *ranges[] = { hRange, sRange };

        cv::calcHist(&hsv, 1, channels, cv::Mat(), hist, 2, histSize, ranges);
        cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);

        return hist;
    }

    void SaveHistogram(const cv::Mat &hist, const std::string &path)
    {
        cv::FileStorage fs(path, cv::FileStorage::WRITE);
        fs << "histogram" << hist;
    }

    cv::Mat LoadHistogram(const std::string &path)
    {
        cv::FileStorage fs(path, cv::FileStorage::READ);
        cv::Mat hist;
        if (fs.isOpened())
        {
            fs["histogram"] >> hist;
        }
        return hist;
    }

} // namespace SH3DS::Vision
