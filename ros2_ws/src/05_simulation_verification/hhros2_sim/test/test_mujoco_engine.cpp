#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "hhros2_sim/mujoco_engine.hpp"

namespace hhros2_sim
{
namespace
{
std::vector<std::string> logical_joint_names()
{
  return {
    "left_hip_pitch_joint", "left_hip_roll_joint",
    "left_hip_yaw_joint", "left_knee_joint",
    "left_ankle_pitch_joint", "left_ankle_roll_joint",
    "right_hip_pitch_joint", "right_hip_roll_joint",
    "right_hip_yaw_joint", "right_knee_joint",
    "right_ankle_pitch_joint", "right_ankle_roll_joint",
    "waist_yaw_joint", "waist_pitch_joint", "head_yaw_joint",
    "left_shoulder_pitch_joint", "left_shoulder_roll_joint",
    "left_shoulder_yaw_joint", "left_elbow_joint",
    "right_shoulder_pitch_joint", "right_shoulder_roll_joint",
    "right_shoulder_yaw_joint", "right_elbow_joint"};
}

MujocoEngineConfig config_for(const std::string & scene)
{
  const std::string description_share =
    ament_index_cpp::get_package_share_directory("hhros2_description");
  MujocoEngineConfig config;
  config.model_path =
    description_share + "/models/Yidong/mjcf/" + scene;
  config.mujoco_joint_names = logical_joint_names();
  config.mujoco_joint_names[14].clear();
  config.initial_positions.assign(config.mujoco_joint_names.size(), 0.0);
  config.sim_rate_hz = 500.0;
  return config;
}

std::vector<double> zeros(const MujocoEngine & engine)
{
  return std::vector<double>(engine.n_joints(), 0.0);
}

TEST(MujocoEngineContract, LoadsTwentyTwoDofPlantIntoTwentyThreeLogicalSlots)
{
  MujocoEngine engine(config_for("scene_safety_rope.xml"));
  EXPECT_EQ(engine.n_joints(), 23U);
  EXPECT_TRUE(engine.has_safety_rope());

  auto position = zeros(engine);
  auto velocity = zeros(engine);
  auto effort = zeros(engine);
  engine.read(position, velocity, effort);

  EXPECT_DOUBLE_EQ(position[14], 0.0);
  EXPECT_NEAR(engine.safety_rope_length(), 0.89, 1.0e-12);
}

TEST(MujocoEngineContract, RopeCommandsHaveDeterministicResetSemantics)
{
  MujocoEngine engine(config_for("scene_safety_rope.xml"));
  const auto command = zeros(engine);

  engine.release_safety_rope();
  engine.step(command, command, command, command, command);
  EXPECT_NEAR(engine.safety_rope_length(), 0.92, 1.0e-12);

  engine.raise_safety_rope();
  engine.step(command, command, command, command, command);
  EXPECT_LT(engine.safety_rope_length(), 0.92);
  EXPECT_GT(engine.safety_rope_length(), 0.89);

  engine.reset();
  EXPECT_NEAR(engine.safety_rope_length(), 0.89, 1.0e-12);
}

TEST(MujocoEngineContract, RejectsMismatchedCommandArrays)
{
  MujocoEngine engine(config_for("scene.xml"));
  const auto command = zeros(engine);
  std::vector<double> short_command(engine.n_joints() - 1, 0.0);

  EXPECT_THROW(
    engine.step(
      short_command, command, command, command, command),
    std::invalid_argument);
}

TEST(MujocoEngineContract, SupportsExplicitNoRopeScene)
{
  MujocoEngine engine(config_for("scene.xml"));
  EXPECT_FALSE(engine.has_safety_rope());
}

TEST(MujocoEngineContract, RejectsDuplicateMappingsBeforeModelStartup)
{
  auto config = config_for("scene.xml");
  config.mujoco_joint_names[1] = config.mujoco_joint_names[0];
  EXPECT_THROW(MujocoEngine(std::move(config)), std::runtime_error);
}
}  // namespace
}  // namespace hhros2_sim
