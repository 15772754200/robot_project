#include "internal/joint_wiring_map.hpp"
#include "internal/motor_protocol_conversions.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{

#define EXPECT_TRUE(condition) \
        do \
        { \
          if (!(condition)) \
          { \
            std::cerr << __FILE__ << ":" << __LINE__ \
                      << ": expected " #condition "\n"; \
            std::exit(1); \
          } \
        } while (false)

bool near(double actual, double expected)
{
  return std::abs(actual - expected) < 1e-5;
}

void test_logical_and_physical_lookups_are_round_trip()
{
  EXPECT_TRUE(joint_wiring::is_valid_wiring_map());
  for (std::size_t logical = 0;
    logical < joint_wiring::kPhysicalMotorMap.size(); ++logical)
  {
    const auto * wiring =
      joint_wiring::wiring_for_logical_joint(logical);
    EXPECT_TRUE(wiring != nullptr);
    EXPECT_TRUE(
      joint_wiring::logical_joint_for_physical_motor(
        wiring->master_id, wiring->motor_index) ==
      static_cast<int>(logical));
  }
  EXPECT_TRUE(
    joint_wiring::wiring_for_logical_joint(
      joint_wiring::kPhysicalMotorMap.size()) == nullptr);
  EXPECT_TRUE(
    joint_wiring::logical_joint_for_physical_motor(3, 0) == -1);
}

void test_feedback_calibration_follows_the_wired_motor_model()
{
  for (const auto & wiring : joint_wiring::kPhysicalMotorMap) {
    const auto expected =
      joint_wiring::calibration_for_model(wiring.motor_model);
    const auto * actual =
      joint_wiring::calibration_for_physical_motor(
      wiring.master_id, wiring.motor_index);
    EXPECT_TRUE(actual != nullptr);
    EXPECT_TRUE(actual->protocol == expected.protocol);
    EXPECT_TRUE(near(actual->torque_constant, expected.torque_constant));
    EXPECT_TRUE(near(actual->command_effort_min, expected.command_effort_min));
    EXPECT_TRUE(near(actual->command_effort_max, expected.command_effort_max));

    if (expected.protocol == joint_wiring::MotorProtocol::Ti5) {
      EXPECT_TRUE(
        near(
          ti5_current_to_effort(
            wiring.master_id, wiring.motor_index, 1.0),
          expected.torque_constant * expected.gear_ratio));
    } else {
      EXPECT_TRUE(
        near(
          encos_pulse_to_effort(4095.0, wiring.motor_index),
          expected.feedback_current_range *
          expected.torque_constant));
    }
  }
}

void test_calibration_is_owned_by_the_physical_motor()
{
  const auto * head_motor =
    joint_wiring::calibration_for_physical_motor(1, 0);
  const auto * shoulder_motor =
    joint_wiring::calibration_for_physical_motor(1, 1);
  EXPECT_TRUE(head_motor != nullptr);
  EXPECT_TRUE(shoulder_motor != nullptr);
  EXPECT_TRUE(near(head_motor->torque_constant, 0.05));
  EXPECT_TRUE(near(shoulder_motor->torque_constant, 0.089));
  EXPECT_TRUE(
    joint_wiring::logical_joint_for_physical_motor(1, 0) == 14);
  EXPECT_TRUE(
    joint_wiring::logical_joint_for_physical_motor(1, 1) == 15);
}

void test_unknown_physical_motor_fails_closed()
{
  EXPECT_TRUE(
    joint_wiring::calibration_for_physical_motor(3, 0) == nullptr);
  EXPECT_TRUE(near(ti5_current_to_effort(3, 0, 10.0), 0.0));
  EXPECT_TRUE(near(encos_pulse_to_effort(4095.0, -1), 0.0));
}

}  // namespace

int main()
{
  test_logical_and_physical_lookups_are_round_trip();
  test_feedback_calibration_follows_the_wired_motor_model();
  test_calibration_is_owned_by_the_physical_motor();
  test_unknown_physical_motor_fails_closed();
  return 0;
}
