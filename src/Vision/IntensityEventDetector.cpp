#include "Vision/IntensityEventDetector.h"

namespace SH3DS::Vision
{

IntensityEventDetector::IntensityEventDetector(IntensityEventConfig config)
    : config_(config), vMax_(0.0), isBlack_(false)
{
}

bool IntensityEventDetector::Update(double vValue, uint64_t frameIndex)
{
    // ------------------------------------------------------------------
    // Step 1: Update the adaptive baseline
    // ------------------------------------------------------------------
    if (vValue > vMax_)
    {
        vMax_ = vValue;
    }
    else
    {
        vMax_ *= config_.vMaxDecay;
    }

    // ------------------------------------------------------------------
    // Step 2: Evaluate state transitions
    // ------------------------------------------------------------------
    if (!isBlack_)
    {
        if (vValue < config_.dropThreshold * vMax_)
        {
            isBlack_ = true;
            events_.push_back({frameIndex, IntensityEventType::Drop});
        }
    }
    else
    {
        if (vValue > config_.raiseThreshold * vMax_)
        {
            isBlack_ = false;
            events_.push_back({frameIndex, IntensityEventType::Raise});
        }
    }

    return isBlack_;
}

bool IntensityEventDetector::IsBlack() const
{
    return isBlack_;
}

void IntensityEventDetector::Reset()
{
    vMax_    = 0.0;
    isBlack_ = false;
    events_.clear();
}

const std::vector<IntensityEvent> &IntensityEventDetector::GetEvents() const
{
    return events_;
}

} // namespace SH3DS::Vision
