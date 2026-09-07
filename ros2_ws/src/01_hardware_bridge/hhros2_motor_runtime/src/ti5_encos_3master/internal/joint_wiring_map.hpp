#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hhros2_motor_protocol/hhros2_shm_layout.hpp"

namespace joint_wiring
{

enum class MotorProtocol : std::uint8_t
{
  Encos,
  Ti5,
};

enum class MotorModel : std::uint8_t
{
  Encos10020,
  Encos8116,
  Encos6408,
  Encos4315,
  Ti5Ri4052,
  Ti5Ri5060,
  Ti5Ri6070,
};

struct MotorCalibration
{
  MotorProtocol protocol;
  float torque_constant;
  float gear_ratio;
  float feedback_current_range;
  float command_effort_min;
  float command_effort_max;
  float kd_max;
};

constexpr MotorCalibration calibration_for_model(MotorModel model)
{
  switch (model) {
    case MotorModel::Encos10020:
      return {MotorProtocol::Encos, 2.5f, 1.0f, 70.0f,
        -150.0f, 150.0f, 50.0f};
    case MotorModel::Encos8116:
      return {MotorProtocol::Encos, 2.35f, 1.0f, 70.0f,
        -150.0f, 150.0f, 5.0f};
    case MotorModel::Encos6408:
      return {MotorProtocol::Encos, 2.35f, 1.0f, 60.0f,
        -60.0f, 60.0f, 5.0f};
    case MotorModel::Encos4315:
      return {MotorProtocol::Encos, 2.8f, 1.0f, 30.0f,
        -70.0f, 70.0f, 5.0f};
    case MotorModel::Ti5Ri4052:
      return {MotorProtocol::Ti5, 0.05f, 51.0f, 0.0f,
        -8.3f, 8.3f, 5.0f};
    case MotorModel::Ti5Ri5060:
      return {MotorProtocol::Ti5, 0.089f, 51.0f, 0.0f,
        -23.0f, 23.0f, 5.0f};
    case MotorModel::Ti5Ri6070:
      return {MotorProtocol::Ti5, 0.092f, 51.0f, 0.0f,
        -42.0f, 42.0f, 5.0f};
  }
  return {MotorProtocol::Encos, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

constexpr std::array<MotorCalibration, 7> kMotorCalibrations{{
  calibration_for_model(MotorModel::Encos10020),
  calibration_for_model(MotorModel::Encos8116),
  calibration_for_model(MotorModel::Encos6408),
  calibration_for_model(MotorModel::Encos4315),
  calibration_for_model(MotorModel::Ti5Ri4052),
  calibration_for_model(MotorModel::Ti5Ri5060),
  calibration_for_model(MotorModel::Ti5Ri6070),
}};

struct MotorWiring
{
  std::uint8_t master_id;
  std::uint8_t motor_index;
  std::uint8_t logical_joint_index;
  MotorModel motor_model;
};

// Physical EtherCAT motor -> canonical logical joint index.
//
// This is the only editable joint/motor routing table. To rebind joints, change
// (normally swap) only logical_joint_index. The model and its Kt, current range,
// gear ratio and effort limits remain properties of the physical motor row.
constexpr std::array<MotorWiring, hhros2::shm::kJointCount> kPhysicalMotorMap{{
  {0, 0, 0, MotorModel::Encos10020},     // left_hip_pitch_joint
  {0, 1, 1, MotorModel::Encos8116},      // left_hip_roll_joint
  {0, 2, 2, MotorModel::Encos6408},      // left_hip_yaw_joint
  {0, 3, 3, MotorModel::Encos10020},     // left_knee_joint
  {0, 4, 4, MotorModel::Encos4315},      // left_ankle_pitch_joint
  {0, 5, 5, MotorModel::Encos4315},      // left_ankle_roll_joint
  {0, 6, 6, MotorModel::Encos10020},     // right_hip_pitch_joint
  {0, 7, 7, MotorModel::Encos8116},      // right_hip_roll_joint
  {0, 8, 8, MotorModel::Encos6408},      // right_hip_yaw_joint
  {0, 9, 9, MotorModel::Encos10020},     // right_knee_joint
  {0, 10, 10, MotorModel::Encos4315},    // right_ankle_pitch_joint
  {0, 11, 11, MotorModel::Encos4315},    // right_ankle_roll_joint
  {1, 0, 14, MotorModel::Ti5Ri4052},     // head_yaw_joint
  {1, 1, 15, MotorModel::Ti5Ri5060},     // left_shoulder_pitch_joint
  {1, 2, 16, MotorModel::Ti5Ri5060},     // left_shoulder_roll_joint
  {1, 3, 17, MotorModel::Ti5Ri4052},     // left_shoulder_yaw_joint
  {1, 4, 18, MotorModel::Ti5Ri4052},     // left_elbow_joint
  {2, 0, 12, MotorModel::Ti5Ri6070},     // waist_yaw_joint
  {2, 1, 20, MotorModel::Ti5Ri5060},     // right_shoulder_roll_joint
  {2, 2, 22, MotorModel::Ti5Ri4052},     // right_elbow_joint
  {2, 3, 21, MotorModel::Ti5Ri4052},     // right_shoulder_yaw_joint
  {2, 4, 19, MotorModel::Ti5Ri5060},     // right_shoulder_pitch_joint
  {2, 5, 13, MotorModel::Ti5Ri6070},     // waist_pitch_joint
}};

constexpr std::array<std::uint8_t, 3> kMotorCountByMaster{{12, 5, 6}};

constexpr bool is_valid_wiring_map()
{
  std::array<std::array<bool, 12>, 3> occupied{};
  std::array<bool, hhros2::shm::kJointCount> logical_joints{};
  for (const auto & wiring : kPhysicalMotorMap) {
    if (wiring.master_id >= kMotorCountByMaster.size() ||
      wiring.motor_index >= kMotorCountByMaster[wiring.master_id] ||
      occupied[wiring.master_id][wiring.motor_index] ||
      wiring.logical_joint_index >= logical_joints.size() ||
      logical_joints[wiring.logical_joint_index])
    {
      return false;
    }
    occupied[wiring.master_id][wiring.motor_index] = true;
    logical_joints[wiring.logical_joint_index] = true;

    const auto protocol =
      calibration_for_model(wiring.motor_model).protocol;
    if ((wiring.master_id == 0 && protocol != MotorProtocol::Encos) ||
      (wiring.master_id != 0 && protocol != MotorProtocol::Ti5))
    {
      return false;
    }
  }

  for (std::size_t master = 0; master < kMotorCountByMaster.size(); ++master) {
    for (std::size_t motor = 0; motor < kMotorCountByMaster[master]; ++motor) {
      if (!occupied[master][motor]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(
  is_valid_wiring_map(),
  "Motor wiring must cover each physical slot and logical joint exactly "
  "once, and use a model compatible with the master protocol");

constexpr const MotorWiring * wiring_for_logical_joint(
  std::size_t logical_joint_index)
{
  for (const auto & wiring : kPhysicalMotorMap) {
    if (wiring.logical_joint_index == logical_joint_index) {
      return &wiring;
    }
  }
  return nullptr;
}

constexpr int logical_joint_for_physical_motor(
  std::uint8_t master_id, std::uint8_t motor_index)
{
  for (const auto & wiring : kPhysicalMotorMap) {
    if (wiring.master_id == master_id &&
      wiring.motor_index == motor_index)
    {
      return static_cast<int>(wiring.logical_joint_index);
    }
  }
  return -1;
}

constexpr const MotorCalibration * calibration_for_physical_motor(
  std::uint8_t master_id, std::uint8_t motor_index)
{
  for (const auto & wiring : kPhysicalMotorMap) {
    if (wiring.master_id == master_id &&
      wiring.motor_index == motor_index)
    {
      return &kMotorCalibrations[
        static_cast<std::size_t>(wiring.motor_model)];
    }
  }
  return nullptr;
}

constexpr MotorModel encos_model_for_device_id(std::uint16_t motor_id)
{
  if (motor_id == 1 || motor_id == 4) {
    return MotorModel::Encos10020;
  }
  if (motor_id == 2) {
    return MotorModel::Encos8116;
  }
  if (motor_id == 3) {
    return MotorModel::Encos6408;
  }
  return MotorModel::Encos4315;
}

constexpr std::size_t kLeftAnklePitch = 4;
constexpr std::size_t kLeftAnkleRoll = 5;
constexpr std::size_t kRightAnklePitch = 10;
constexpr std::size_t kRightAnkleRoll = 11;

}  // namespace joint_wiring
