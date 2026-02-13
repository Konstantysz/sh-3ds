#include "TemplateMatcher.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace SH3DS::Vision
{
    double TemplateMatcher::Match(const cv::Mat &region, const std::string &templatePath)
    {
        auto it = cache.find(templatePath);
        if (it == cache.end())
        {
            cv::Mat tmpl = cv::imread(templatePath, cv::IMREAD_COLOR);
            if (tmpl.empty())
            {
                return 0.0;
            }
            cache[templatePath] = tmpl;
            it = cache.find(templatePath);
        }

        const cv::Mat &tmpl = it->second;
        cv::Mat resized;
        if (tmpl.size() != region.size())
        {
            cv::resize(tmpl, resized, region.size());
        }
        else
        {
            resized = tmpl;
        }

        cv::Mat result;
        cv::matchTemplate(region, resized, result, cv::TM_CCORR_NORMED);

        double maxVal;
        cv::minMaxLoc(result, nullptr, &maxVal);
        return maxVal;
    }
} // namespace SH3DS::Vision
