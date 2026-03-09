#include "Vision/IntensityEventDetector.h"

namespace SH3DS::Vision
{

    IntensityEventDetector::IntensityEventDetector(IntensityEventConfig config)
        : config(config),
          vMax(0.0),
          isBlack(false)
    {
    }

    bool IntensityEventDetector::Update(double vValue, uint64_t frameIndex)
    {
        // ------------------------------------------------------------------
        // Step 1: Update the adaptive baseline
        // ------------------------------------------------------------------
        if (vValue > vMax)
        {
            vMax = vValue;
        }
        else
        {
            vMax *= config.vMaxDecay;
        }

        // ------------------------------------------------------------------
        // Step 2: Evaluate state transitions
        // ------------------------------------------------------------------
        if (!isBlack)
        {
            if (vValue < config.dropThreshold * vMax)
            {
                isBlack = true;
                events.push_back({ frameIndex, IntensityEventType::Drop });
            }
        }
        else
        {
            if (vValue > config.raiseThreshold * vMax)
            {
                isBlack = false;
                events.push_back({ frameIndex, IntensityEventType::Raise });
            }
        }

        return isBlack;
    }

    bool IntensityEventDetector::IsBlack() const
    {
        return isBlack;
    }

    void IntensityEventDetector::Reset()
    {
        vMax = 0.0;
        isBlack = false;
        events.clear();
    }

    const std::vector<IntensityEvent> &IntensityEventDetector::GetEvents() const
    {
        return events;
    }

} // namespace SH3DS::Vision
