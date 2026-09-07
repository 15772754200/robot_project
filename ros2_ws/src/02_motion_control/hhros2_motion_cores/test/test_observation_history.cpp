#include <vector>

#include <gtest/gtest.h>

#include "hhros2_motion_cores/rl/gait/gait_utils.hpp"
#include "hhros2_motion_cores/rl/observation/observation_builder.hpp"

namespace hhros2_motion_cores::rl_observation
{
namespace
{

ObservationConfig CommandHistoryConfig(int initialization_mode)
{
    ObservationConfig config;
    config.num_observations = 3;
    config.observation_stack = 3;
    config.observation_stack_mode = initialization_mode;
    config.observations = {"command"};
    return config;
}

ObservationInput CommandInput(float first)
{
    ObservationInput input;
    input.command = {first, first + 1.0F, first + 2.0F};
    return input;
}

TEST(ObservationHistory, ZeroPrefillIsOldestToNewest)
{
    ObservationBuilder builder(CommandHistoryConfig(1));

    EXPECT_EQ(
        builder.Build(CommandInput(1.0F)),
        (std::vector<float>{0.0F, 0.0F, 0.0F,
                            0.0F, 0.0F, 0.0F,
                            1.0F, 2.0F, 3.0F}));
    EXPECT_EQ(
        builder.Build(CommandInput(4.0F)),
        (std::vector<float>{0.0F, 0.0F, 0.0F,
                            1.0F, 2.0F, 3.0F,
                            4.0F, 5.0F, 6.0F}));
}

TEST(ObservationHistory, RepeatCurrentStillUsesOldestToNewest)
{
    ObservationBuilder builder(CommandHistoryConfig(0));

    EXPECT_EQ(
        builder.Build(CommandInput(1.0F)),
        (std::vector<float>{1.0F, 2.0F, 3.0F,
                            1.0F, 2.0F, 3.0F,
                            1.0F, 2.0F, 3.0F}));
    EXPECT_EQ(
        builder.Build(CommandInput(4.0F)),
        (std::vector<float>{1.0F, 2.0F, 3.0F,
                            1.0F, 2.0F, 3.0F,
                            4.0F, 5.0F, 6.0F}));
}

TEST(ObservationHistory, LegacyModeSerializesHistoryByObservationTerm)
{
    auto config = CommandHistoryConfig(0);
    config.num_observations = 4;
    config.observation_stack = 2;
    config.observations = {"command", "period"};
    ObservationBuilder builder(config);

    auto first = CommandInput(1.0F);
    first.period = {0.5F};
    EXPECT_EQ(
        builder.Build(first),
        (std::vector<float>{1.0F, 2.0F, 3.0F,
                            1.0F, 2.0F, 3.0F,
                            0.5F, 0.5F}));

    auto second = CommandInput(4.0F);
    second.period = {0.8F};
    EXPECT_EQ(
        builder.Build(second),
        (std::vector<float>{1.0F, 2.0F, 3.0F,
                            4.0F, 5.0F, 6.0F,
                            0.5F, 0.8F}));
}

TEST(GaitObservation, PeriodIsCycleTimeNotNormalizedPhase)
{
    const auto phase = rl_gait::MakeGaitPhase(1.0, 0.8);

    EXPECT_FLOAT_EQ(rl_gait::PeriodObservation(phase).at(0), 0.8F);
    EXPECT_NEAR(phase.normalized, 0.25, 1e-12);
}

TEST(GaitObservation, InvalidPeriodIsDisabled)
{
    const auto phase = rl_gait::MakeGaitPhase(1.0, -0.8);

    EXPECT_FLOAT_EQ(rl_gait::PeriodObservation(phase).at(0), 0.0F);
    EXPECT_DOUBLE_EQ(phase.normalized, 0.0);
}

}  // namespace
}  // namespace hhros2_motion_cores::rl_observation
