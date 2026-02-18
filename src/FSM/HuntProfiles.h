#pragma once

#include "FSM/CXXStateTreeFSM.h"

#include <memory>
#include <string>

namespace SH3DS::FSM
{
    /**
     * @brief Factory for creating hunt-specific FSM instances.
     *
     * Each static method builds a CXXStateTreeFSM with hardcoded states/transitions
     * for a specific hunt method. Detection parameters (HSV ranges, thresholds)
     * come from YAML via HuntDetectionParams.
     */
    class HuntProfiles
    {
    public:
        /**
         * @brief Creates the XY Starter Soft Reset FSM.
         *
         * States: load_game -> game_start -> cutscene_start -> cutscene ->
         *         starter_pick -> nickname_prompt -> post_selection ->
         *         cutscene_end -> party_menu -> pokemon_summary -> soft_reset -> (loop)
         *
         * @param params Detection parameters loaded from YAML.
         * @return Configured FSM instance.
         */
        static std::unique_ptr<CXXStateTreeFSM> CreateXYStarterSR(const Core::HuntDetectionParams &params);
    };
} // namespace SH3DS::FSM
