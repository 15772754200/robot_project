#pragma once

// ROS-independent MuJoCo physics owner for the simulation hardware plugin.
// Joint arrays use the canonical ros2_control order; an empty MuJoCo name marks
// the one logical joint that is intentionally absent from the training plant.

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// Forward declarations keep mujoco.h out of this header so packages that only
// reference the engine type do not need the MuJoCo include path.
struct mjModel_;
struct mjData_;
typedef struct mjModel_ mjModel;
typedef struct mjData_ mjData;

namespace hhros2_sim
{

struct MujocoEngineConfig
{
  std::string model_path;
  std::string floating_base_joint_name = "base_free_joint";
  // Indexed by ros2_control joint order. Empty means no MJCF counterpart.
  std::vector<std::string> mujoco_joint_names;
  std::vector<double> initial_positions;
  std::array<double, 3> initial_base_position{{0.0, 0.0, 0.80}};
  std::array<double, 4> initial_base_orientation_wxyz{{1.0, 0.0, 0.0, 0.0}};
  double sim_rate_hz = 1000.0;
};

struct ImuSnapshot
{
  std::array<double, 4> orientation_wxyz{{1.0, 0.0, 0.0, 0.0}};
  std::array<double, 3> angular_velocity{{0.0, 0.0, 0.0}};
  std::array<double, 3> linear_acceleration{{0.0, 0.0, 0.0}};
};

struct BasePoseSnapshot
{
  std::array<double, 3> position{{0.0, 0.0, 0.0}};
  std::array<double, 4> orientation_wxyz{{1.0, 0.0, 0.0, 0.0}};
};

class MujocoEngine
{
public:
  explicit MujocoEngine(MujocoEngineConfig config);
  ~MujocoEngine();

  MujocoEngine(const MujocoEngine &) = delete;
  MujocoEngine & operator=(const MujocoEngine &) = delete;

  void reset();

  // Hybrid command, all arrays indexed by ROS joint order (size n_joints()).
  void step(
    const std::vector<double> & q_des,
    const std::vector<double> & dq_des,
    const std::vector<double> & tau_ff,
    const std::vector<double> & kp,
    const std::vector<double> & kd);

  // Measured joint feedback, indexed by ROS joint order.
  void read(
    std::vector<double> & position,
    std::vector<double> & velocity,
    std::vector<double> & effort) const;

  ImuSnapshot imu() const;

  bool has_safety_rope() const {return safety_rope_tendon_id_ >= 0;}
  double safety_rope_length() const {return safety_rope_length_;}
  void raise_safety_rope();
  void release_safety_rope();

  std::size_t n_joints() const {return config_.mujoco_joint_names.size();}
  double sim_period_sec() const {return 1.0 / config_.sim_rate_hz;}
  BasePoseSnapshot base_pose() const;

private:
  struct JointBinding
  {
    int ros_index = -1;
    int qpos_adr = -1;
    int qvel_adr = -1;
    int actuator_id = -1;
  };

  void load_model();
  void resolve_sensors();
  void resolve_bindings();
  void resolve_safety_rope();
  void set_initial_state();
  void update_safety_trolley();
  void update_safety_rope();
  void destroy_model() noexcept;
  void validate_joint_array_size(
    const std::vector<double> & values, const char * name) const;
  int joint_qpos_width(int joint_id) const;
  int joint_qvel_width(int joint_id) const;
  int resolve_joint_actuator(int joint_id, const std::string & name) const;

  MujocoEngineConfig config_;
  mjModel * model_ = nullptr;
  mjData * data_ = nullptr;
  std::vector<JointBinding> bindings_;
  int base_qpos_adr_ = -1;
  int base_qvel_adr_ = -1;
  std::vector<double> initial_qpos_;
  std::vector<double> initial_qvel_;
  int orientation_sensor_id_ = -1;
  int angular_velocity_sensor_id_ = -1;
  int linear_acceleration_sensor_id_ = -1;
  int safety_rope_tendon_id_ = -1;
  int safety_rope_harness_site_id_ = -1;
  int safety_rope_trolley_mocap_id_ = -1;
  double safety_rope_length_ = 0.0;
  double safety_rope_target_length_ = 0.0;
};

}  // namespace hhros2_sim
