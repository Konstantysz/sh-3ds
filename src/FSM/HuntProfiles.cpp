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
                throw std::runtime_error("HuntProfiles: missing detection params for required state '" + stateId
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
            .transitionsTo = { "game_start" },
            .maxDurationS = 15,
            .detectionParameters = RequireState(params, "load_game"),
        });

        builder.AddState({
            .id = "game_start",
            .transitionsTo = { "cutscene_part_1" },
            .maxDurationS = 10,
            .detectionParameters = RequireState(params, "game_start"),
        });

        builder.AddState({
            .id = "cutscene_part_1",
            .transitionsTo = { "starter_pick" },
            .maxDurationS = 120,
            .detectionParameters = RequireState(params, "cutscene_part_1"),
        });

        builder.AddState({
            .id = "starter_pick",
            .transitionsTo = { "cutscene_part_2" },
            .maxDurationS = 15,
            .detectionParameters = RequireState(params, "starter_pick"),
        });

        builder.AddState({
            .id = "cutscene_part_2",
            .transitionsTo = { "nickname_prompt" },
            .maxDurationS = 120,
            .detectionParameters = RequireState(params, "cutscene_part_2"),
        });

        builder.AddState({
            .id = "nickname_prompt",
            .transitionsTo = { "cutscene_part_3" },
            .maxDurationS = 10,
            .detectionParameters = RequireState(params, "nickname_prompt"),
        });

        builder.AddState({
            .id = "cutscene_part_3",
            .transitionsTo = { "party_menu" },
            .maxDurationS = 120,
            .detectionParameters = RequireState(params, "cutscene_part_3"),
        });

        builder.AddState({
            .id = "party_menu",
            .transitionsTo = { "pokemon_summary" },
            .maxDurationS = 10,
            .detectionParameters = RequireState(params, "party_menu"),
        });

        builder.AddState({
            .id = "pokemon_summary",
            .transitionsTo = { "soft_reset" },
            .maxDurationS = 20,
            .shinyCheck = true,
            .detectionParameters = RequireState(params, "pokemon_summary"),
        });

        builder.AddState({
            .id = "soft_reset",
            .transitionsTo = { "load_game" },
            .maxDurationS = 15,
            .detectionParameters = RequireState(params, "soft_reset"),
        });

        return builder.Build();
    }
} // namespace SH3DS::FSM
