#pragma once

#include <cstdint>
#include <vector>

namespace SH3DS::Vision
{
    /**
     * @brief Discriminates the direction of an intensity transition.
     */
    enum class IntensityEventType
    {
        Drop,  ///< Screen faded to black (V fell below dropThreshold * vMax)
        Raise, ///< Screen recovered from black (V rose above raiseThreshold * vMax)
    };

    /**
     * @brief A single detected intensity transition.
     */
    struct IntensityEvent
    {
        uint64_t frameIndex;     ///< Frame number at which the transition occurred
        IntensityEventType type; ///< Direction of the transition
    };

    /**
     * @brief Tuning parameters for IntensityEventDetector.
     */
    struct IntensityEventConfig
    {
        double dropThreshold = 0.40;  ///< V < dropThreshold  * vMax  -> DROP
        double raiseThreshold = 0.50; ///< V > raiseThreshold * vMax  -> RAISE
        double vMaxDecay = 0.9995;    ///< Per-frame exponential decay of the baseline (initialised to 0 — ramps up from first frame)
    };

    /**
     * @brief Causal state machine that tracks a slowly-decaying brightness baseline
     *        and emits DROP / RAISE events when the V channel crosses adaptive thresholds.
     *
     * Feed each frame's average V value (in [0, 1]) into Update().  The detector
     * maintains an internal `vMax` baseline: it rises instantly to any new peak
     * and decays exponentially otherwise.  A transition from "bright" to "black"
     * emits a Drop event; the reverse emits a Raise event.  Only one event type
     * is emitted per transition (no double-DROP / double-RAISE).
     */
    class IntensityEventDetector
    {
    public:
        /**
         * @brief Constructs the detector with the given configuration.
         * @param config Tuning parameters (defaults give sensible results).
         */
        explicit IntensityEventDetector(IntensityEventConfig config = {});

        /**
         * @brief Advances the state machine with a new V-channel sample.
         * @param vValue    Average V value for this frame, in [0, 1].
         * @param frameIndex Frame counter used to timestamp events.
         * @return True, if the screen is currently considered "black".
         */
        bool Update(double vValue, uint64_t frameIndex);

        /**
         * @brief Returns true if the screen is currently considered "black".
         */
        bool IsBlack() const;

        /**
         * @brief Resets the state machine (clears events and baseline).
         */
        void Reset();

        /**
         * @brief Returns the full event history recorded since construction or last Reset().
         */
        [[nodiscard]] const std::vector<IntensityEvent> &GetEvents() const;

    private:
        IntensityEventConfig config;        ///< Tuning parameters
        double vMax;                        ///< Adaptive brightness baseline
        bool isBlack;                       ///< Current black-screen state
        std::vector<IntensityEvent> events; ///< Recorded transition events
    };

} // namespace SH3DS::Vision
