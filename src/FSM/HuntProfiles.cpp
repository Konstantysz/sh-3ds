#include "HuntProfiles.h"

#include <stdexcept>

namespace SH3DS::FSM
{
    namespace
    {
        const Core::StateDetectionParams &RequireState(const Core::HuntDetectionParams &params,
            const std::string &stateId)
        {
            auto it = params.stateParams.find(stateId);
            if (it == params.stateParams.end())
            {
                throw std::runtime_error(
                    "HuntProfiles: missing detection params for required state '" + stateId
                    + "'. Check fsm_states in your hunt config YAML.");
            }
            return it->second;
        }
    } // namespace

    std::unique_ptr<CXXStateTreeFSM> HuntProfiles::CreateXYStarterSR(const Core::HuntDetectionParams &params)
    {
        CXXStateTreeFSM::Builder builder;
        builder.SetInitialState("load_game");
        builder.SetDebounceFrames(params.debounceFrames);

        builder.AddState({
            .id = "load_game",
            .transitionsTo = {"game_start", "soft_reset"},
            .maxDurationS = 15,
            .detection = RequireState(params, "load_game"),
        });

        builder.AddState({
            .id = "game_start",
            .transitionsTo = {"cutscene_start", "soft_reset"},
            .maxDurationS = 10,
            .detection = RequireState(params, "game_start"),
        });

        builder.AddState({
            .id = "cutscene_start",
            .transitionsTo = {"cutscene", "soft_reset"},
            .maxDurationS = 10,
            .detection = RequireState(params, "cutscene_start"),
        });

        builder.AddState({
            .id = "cutscene",
            .transitionsTo = {"starter_pick", "soft_reset"},
            .maxDurationS = 30,
            .detection = RequireState(params, "cutscene"),
        });

        builder.AddState({
            .id = "starter_pick",
            .transitionsTo = {"nickname_prompt", "soft_reset"},
            .maxDurationS = 15,
            .detection = RequireState(params, "starter_pick"),
        });

        builder.AddState({
            .id = "nickname_prompt",
            .transitionsTo = {"post_selection", "soft_reset"},
            .maxDurationS = 10,
            .detection = RequireState(params, "nickname_prompt"),
        });

        builder.AddState({
            .id = "post_selection",
            .transitionsTo = {"cutscene_end", "soft_reset"},
            .maxDurationS = 15,
            .detection = RequireState(params, "post_selection"),
        });

        builder.AddState({
            .id = "cutscene_end",
            .transitionsTo = {"party_menu", "soft_reset"},
            .maxDurationS = 15,
            .detection = RequireState(params, "cutscene_end"),
        });

        builder.AddState({
            .id = "party_menu",
            .transitionsTo = {"pokemon_summary", "soft_reset"},
            .maxDurationS = 10,
            .detection = RequireState(params, "party_menu"),
        });

        builder.AddState({
            .id = "pokemon_summary",
            .transitionsTo = {"soft_reset"},
            .maxDurationS = 20,
            .shinyCheck = true,
            .detection = RequireState(params, "pokemon_summary"),
        });

        builder.AddState({
            .id = "soft_reset",
            .transitionsTo = {"load_game"},
            .maxDurationS = 15,
            .detection = RequireState(params, "soft_reset"),
        });

        return builder.Build();
    }
} // namespace SH3DS::FSM
