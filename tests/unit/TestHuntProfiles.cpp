#include "FSM/HuntProfiles.h"

#include <gtest/gtest.h>

namespace
{
    SH3DS::Core::HuntDetectionParams MakeCompleteParams()
    {
        SH3DS::Core::StateDetectionParams defaultSp{
            .roi = "full_screen",
            .method = "color_histogram",
            .hsvLower = cv::Scalar(0, 0, 0),
            .hsvUpper = cv::Scalar(180, 255, 255),
            .pixelRatioMin = 0.0,
            .pixelRatioMax = 1.0,
            .threshold = 0.5,
            .templatePath = {},
        };

        SH3DS::Core::HuntDetectionParams params;
        params.debounceFrames = 2;
        for (const auto &id : { "load_game",
                 "game_start",
                 "cutscene_part_1",
                 "starter_pick",
                 "cutscene_part_2",
                 "nickname_prompt",
                 "cutscene_part_3",
                 "party_menu",
                 "pokemon_summary",
                 "soft_reset" })
        {
            params.stateParams[id] = defaultSp;
        }
        return params;
    }

} // namespace

TEST(HuntProfiles, CreateXYStarterSRSucceedsWithCompleteParams)
{
    auto params = MakeCompleteParams();
    EXPECT_NO_THROW({
        auto fsm = SH3DS::FSM::HuntProfiles::CreateXYStarterSR(params);
        EXPECT_EQ(fsm->GetCurrentState(), "load_game");
    });
}

TEST(HuntProfiles, CreateXYStarterSRThrowsDescriptiveErrorForMissingState)
{
    auto params = MakeCompleteParams();
    params.stateParams.erase("pokemon_summary"); // remove a required state

    try
    {
        SH3DS::FSM::HuntProfiles::CreateXYStarterSR(params);
        FAIL() << "Expected std::runtime_error for missing state";
    }
    catch (const std::runtime_error &e)
    {
        EXPECT_NE(std::string(e.what()).find("pokemon_summary"), std::string::npos)
            << "Error message should name the missing state; got: " << e.what();
    }
}
