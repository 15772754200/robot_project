#include "hhros2_sim/mujoco_engine.hpp"

#include <mujoco/mujoco.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace hhros2_sim
{
namespace
{
constexpr char kSafetyRopeTendonName[] = "safety_rope";
constexpr char kSafetyRopeTrolleyBodyName[] = "safety_trolley";
constexpr char kSafetyRopeHarnessSiteName[] = "safety_harness";
constexpr double kSafetyRopeHeldLength = 0.89;
constexpr double kSafetyRopeReleasedLength = 0.92;
constexpr double kSafetyRopeRaiseSpeed = 0.10;
}  // namespace

MujocoEngine::MujocoEngine(MujocoEngineConfig config)
: config_(std::move(config))
{
  if (config_.model_path.empty()) {
    throw std::runtime_error("model_path must not be empty");
  }
  if (config_.floating_base_joint_name.empty()) {
    throw std::runtime_error("floating_base_joint_name must not be empty");
  }
  if (!std::isfinite(config_.sim_rate_hz) || config_.sim_rate_hz <= 0.0) {
    throw std::runtime_error("sim_rate_hz must be finite and positive");
  }
  if (config_.mujoco_joint_names.empty() ||
    config_.initial_positions.size() != config_.mujoco_joint_names.size())
  {
    throw std::runtime_error(
            "MuJoCo joint names and initial positions must have equal, nonzero size");
  }
  std::unordered_set<std::string> unique_joint_names;
  std::size_t mapped_joint_count = 0;
  for (std::size_t i = 0; i < config_.mujoco_joint_names.size(); ++i) {
    const auto & joint_name = config_.mujoco_joint_names[i];
    if (!std::isfinite(config_.initial_positions[i])) {
      throw std::runtime_error("Initial joint positions must be finite");
    }
    if (joint_name.empty()) {
      continue;
    }
    ++mapped_joint_count;
    if (!unique_joint_names.insert(joint_name).second) {
      throw std::runtime_error(
              "MuJoCo joint mappings must be unique: " + joint_name);
    }
  }
  if (mapped_joint_count == 0) {
    throw std::runtime_error("At least one MuJoCo joint mapping is required");
  }
  for (const double value : config_.initial_base_position) {
    if (!std::isfinite(value)) {
      throw std::runtime_error("Initial base position must be finite");
    }
  }
  double quaternion_norm_squared = 0.0;
  for (const double value : config_.initial_base_orientation_wxyz) {
    if (!std::isfinite(value)) {
      throw std::runtime_error("Initial base orientation must be finite");
    }
    quaternion_norm_squared += value * value;
  }
  if (quaternion_norm_squared <= 1.0e-24) {
    throw std::runtime_error("Initial base orientation must be non-zero");
  }
  const double quaternion_norm = std::sqrt(quaternion_norm_squared);
  for (double & value : config_.initial_base_orientation_wxyz) {
    value /= quaternion_norm;
  }

  try {
    load_model();
    resolve_sensors();
    resolve_bindings();
    resolve_safety_rope();
    set_initial_state();
  } catch (...) {
    destroy_model();
    throw;
  }
}

MujocoEngine::~MujocoEngine()
{
  destroy_model();
}

void MujocoEngine::destroy_model() noexcept
{
  if (data_ != nullptr) {
    mj_deleteData(data_);
  }
  if (model_ != nullptr) {
    mj_deleteModel(model_);
  }
  data_ = nullptr;
  model_ = nullptr;
}

void MujocoEngine::load_model()
{
  char error[1024] = {0};
  model_ = mj_loadXML(config_.model_path.c_str(), nullptr, error, sizeof(error));
  if (model_ == nullptr) {
    throw std::runtime_error(
            "Failed to load MuJoCo model '" + config_.model_path +
            "': " + std::string(error));
  }
  data_ = mj_makeData(model_);
  if (data_ == nullptr) {
    throw std::runtime_error(
            "Failed to allocate MuJoCo data for model: " + config_.model_path);
  }
  model_->opt.timestep = sim_period_sec();
}

void MujocoEngine::resolve_sensors()
{
  auto resolve = [&](const char * name, int dim) -> int {
    const int id = mj_name2id(model_, mjOBJ_SENSOR, name);
    if (id < 0) {
      throw std::runtime_error(
              std::string("Missing MuJoCo IMU sensor: ") + name);
    }
    if (model_->sensor_dim[id] != dim) {
      throw std::runtime_error(
              std::string("MuJoCo sensor has unexpected dim: ") + name);
    }
    return id;
  };
  orientation_sensor_id_ = resolve("orientation", 4);
  angular_velocity_sensor_id_ = resolve("angular-velocity", 3);
  linear_acceleration_sensor_id_ = resolve("linear-acceleration", 3);
}

int MujocoEngine::joint_qpos_width(int joint_id) const
{
  const int start = model_->jnt_qposadr[joint_id];
  const int end = (joint_id + 1 < model_->njnt) ?
    model_->jnt_qposadr[joint_id + 1] :
    model_->nq;
  return end - start;
}

int MujocoEngine::joint_qvel_width(int joint_id) const
{
  const int start = model_->jnt_dofadr[joint_id];
  const int end = (joint_id + 1 < model_->njnt) ?
    model_->jnt_dofadr[joint_id + 1] :
    model_->nv;
  return end - start;
}

int MujocoEngine::resolve_joint_actuator(
  int joint_id, const std::string & name) const
{
  const int named = mj_name2id(model_, mjOBJ_ACTUATOR, name.c_str());
  if (named >= 0 && model_->actuator_trntype[named] == mjTRN_JOINT &&
    model_->actuator_trnid[2 * named] == joint_id)
  {
    return named;
  }
  for (int a = 0; a < model_->nu; ++a) {
    if (model_->actuator_trntype[a] != mjTRN_JOINT) {continue;}
    if (model_->actuator_trnid[2 * a] == joint_id) {return a;}
  }
  throw std::runtime_error("Missing MuJoCo actuator for joint: " + name);
}

void MujocoEngine::resolve_bindings()
{
  bindings_.clear();
  const std::size_t n = config_.mujoco_joint_names.size();
  for (std::size_t i = 0; i < n; ++i) {
    const std::string & mj_name = config_.mujoco_joint_names[i];
    if (mj_name.empty()) {continue;}    // ROS joint with no MJCF counterpart

    const int jid = mj_name2id(model_, mjOBJ_JOINT, mj_name.c_str());
    if (jid < 0) {
      throw std::runtime_error("Missing MuJoCo joint: " + mj_name);
    }
    if (joint_qpos_width(jid) != 1 || joint_qvel_width(jid) != 1) {
      throw std::runtime_error("MuJoCo joint is not 1-DoF: " + mj_name);
    }
    bindings_.push_back(
      JointBinding{
        static_cast<int>(i), model_->jnt_qposadr[jid],
        model_->jnt_dofadr[jid], resolve_joint_actuator(jid, mj_name)});
  }

  const int base_jid =
    mj_name2id(model_, mjOBJ_JOINT, config_.floating_base_joint_name.c_str());
  if (base_jid < 0) {
    throw std::runtime_error(
            "Floating base joint not found: " + config_.floating_base_joint_name);
  }
  if (joint_qpos_width(base_jid) != 7 || joint_qvel_width(base_jid) != 6) {
    throw std::runtime_error(
            "Floating base joint must be a freejoint: " +
            config_.floating_base_joint_name);
  }
  base_qpos_adr_ = model_->jnt_qposadr[base_jid];
  base_qvel_adr_ = model_->jnt_dofadr[base_jid];
}

void MujocoEngine::set_initial_state()
{
  data_->qpos[base_qpos_adr_ + 0] = config_.initial_base_position[0];
  data_->qpos[base_qpos_adr_ + 1] = config_.initial_base_position[1];
  data_->qpos[base_qpos_adr_ + 2] = config_.initial_base_position[2];
  data_->qpos[base_qpos_adr_ + 3] = config_.initial_base_orientation_wxyz[0];
  data_->qpos[base_qpos_adr_ + 4] = config_.initial_base_orientation_wxyz[1];
  data_->qpos[base_qpos_adr_ + 5] = config_.initial_base_orientation_wxyz[2];
  data_->qpos[base_qpos_adr_ + 6] = config_.initial_base_orientation_wxyz[3];
  for (int i = 0; i < 6; ++i) {
    data_->qvel[base_qvel_adr_ + i] = 0.0;
  }

  for (const auto & b : bindings_) {
    data_->qpos[b.qpos_adr] = config_.initial_positions[b.ros_index];
    data_->qvel[b.qvel_adr] = 0.0;
  }
  mj_forward(model_, data_);
  update_safety_trolley();
  mj_forward(model_, data_);
  initial_qpos_.assign(data_->qpos, data_->qpos + model_->nq);
  initial_qvel_.assign(data_->qvel, data_->qvel + model_->nv);
}

void MujocoEngine::reset()
{
  std::copy(initial_qpos_.begin(), initial_qpos_.end(), data_->qpos);
  std::copy(initial_qvel_.begin(), initial_qvel_.end(), data_->qvel);
  data_->time = 0.0;
  std::fill(data_->qfrc_applied, data_->qfrc_applied + model_->nv, 0.0);
  std::fill(data_->ctrl, data_->ctrl + model_->nu, 0.0);
  safety_rope_length_ = kSafetyRopeHeldLength;
  safety_rope_target_length_ = safety_rope_length_;
  if (has_safety_rope()) {
    model_->tendon_lengthspring[2 * safety_rope_tendon_id_ + 1] =
      safety_rope_length_;
  }
  mj_forward(model_, data_);
  update_safety_trolley();
  mj_forward(model_, data_);
}

void MujocoEngine::resolve_safety_rope()
{
  safety_rope_tendon_id_ = mj_name2id(
    model_, mjOBJ_TENDON, kSafetyRopeTendonName);
  if (safety_rope_tendon_id_ < 0) {
    return;
  }

  safety_rope_harness_site_id_ = mj_name2id(
    model_, mjOBJ_SITE, kSafetyRopeHarnessSiteName);
  const int trolley_body_id = mj_name2id(
    model_, mjOBJ_BODY, kSafetyRopeTrolleyBodyName);
  if (safety_rope_harness_site_id_ < 0 || trolley_body_id < 0 ||
    model_->body_mocapid[trolley_body_id] < 0)
  {
    throw std::runtime_error(
            "Safety rope requires the configured harness site and mocap trolley");
  }
  safety_rope_trolley_mocap_id_ = model_->body_mocapid[trolley_body_id];

  safety_rope_length_ = kSafetyRopeHeldLength;
  safety_rope_target_length_ = safety_rope_length_;
  model_->tendon_lengthspring[2 * safety_rope_tendon_id_ + 1] =
    safety_rope_length_;
}

void MujocoEngine::raise_safety_rope()
{
  if (!has_safety_rope()) {return;}
  safety_rope_target_length_ = kSafetyRopeHeldLength;
}

void MujocoEngine::release_safety_rope()
{
  if (!has_safety_rope()) {return;}
  safety_rope_target_length_ = kSafetyRopeReleasedLength;
}

void MujocoEngine::update_safety_trolley()
{
  if (safety_rope_harness_site_id_ < 0 || safety_rope_trolley_mocap_id_ < 0) {
    return;
  }

  mj_kinematics(model_, data_);
  const double * harness =
    data_->site_xpos + 3 * safety_rope_harness_site_id_;
  double * trolley = data_->mocap_pos + 3 * safety_rope_trolley_mocap_id_;
  trolley[0] = harness[0];
  trolley[1] = harness[1];
}

void MujocoEngine::update_safety_rope()
{
  if (!has_safety_rope()) {return;}

  update_safety_trolley();
  if (safety_rope_target_length_ >= safety_rope_length_) {
    safety_rope_length_ = safety_rope_target_length_;
  } else {
    const double max_step =
      kSafetyRopeRaiseSpeed * sim_period_sec();
    safety_rope_length_ = std::max(
      safety_rope_target_length_, safety_rope_length_ - max_step);
  }
  model_->tendon_lengthspring[2 * safety_rope_tendon_id_ + 1] =
    safety_rope_length_;
}

void MujocoEngine::step(
  const std::vector<double> & q_des,
  const std::vector<double> & dq_des,
  const std::vector<double> & tau_ff,
  const std::vector<double> & kp,
  const std::vector<double> & kd)
{
  validate_joint_array_size(q_des, "q_des");
  validate_joint_array_size(dq_des, "dq_des");
  validate_joint_array_size(tau_ff, "tau_ff");
  validate_joint_array_size(kp, "kp");
  validate_joint_array_size(kd, "kd");
  update_safety_rope();
  std::fill(data_->qfrc_applied, data_->qfrc_applied + model_->nv, 0.0);
  std::fill(data_->ctrl, data_->ctrl + model_->nu, 0.0);

  for (const auto & b : bindings_) {
    const int i = b.ros_index;
    const double q = data_->qpos[b.qpos_adr];
    const double dq = data_->qvel[b.qvel_adr];
    const double qd = std::isnan(q_des[i]) ? q : q_des[i];
    data_->ctrl[b.actuator_id] =
      tau_ff[i] + kp[i] * (qd - q) + kd[i] * (dq_des[i] - dq);
  }
  mj_step(model_, data_);
}

void MujocoEngine::read(
  std::vector<double> & position,
  std::vector<double> & velocity,
  std::vector<double> & effort) const
{
  validate_joint_array_size(position, "position");
  validate_joint_array_size(velocity, "velocity");
  validate_joint_array_size(effort, "effort");
  std::copy(
    config_.initial_positions.begin(), config_.initial_positions.end(),
    position.begin());
  std::fill(velocity.begin(), velocity.end(), 0.0);
  std::fill(effort.begin(), effort.end(), 0.0);
  for (const auto & b : bindings_) {
    const int i = b.ros_index;
    position[i] = data_->qpos[b.qpos_adr];
    velocity[i] = data_->qvel[b.qvel_adr];
    effort[i] = data_->actuator_force[b.actuator_id];
  }
}

void MujocoEngine::validate_joint_array_size(
  const std::vector<double> & values, const char * name) const
{
  if (values.size() != n_joints()) {
    throw std::invalid_argument(
            std::string(name) + " must contain exactly " +
            std::to_string(n_joints()) + " joint values");
  }
}

ImuSnapshot MujocoEngine::imu() const
{
  ImuSnapshot s;
  const double * ori =
    data_->sensordata + model_->sensor_adr[orientation_sensor_id_];
  s.orientation_wxyz = {{ori[0], ori[1], ori[2], ori[3]}};
  const double norm = std::sqrt(
    s.orientation_wxyz[0] * s.orientation_wxyz[0] +
    s.orientation_wxyz[1] * s.orientation_wxyz[1] +
    s.orientation_wxyz[2] * s.orientation_wxyz[2] +
    s.orientation_wxyz[3] * s.orientation_wxyz[3]);
  if (norm > 1.0e-12) {
    for (auto & c : s.orientation_wxyz) {
      c /= norm;
    }
  }

  const double * gyro =
    data_->sensordata + model_->sensor_adr[angular_velocity_sensor_id_];
  s.angular_velocity = {{gyro[0], gyro[1], gyro[2]}};

  const double * acc =
    data_->sensordata + model_->sensor_adr[linear_acceleration_sensor_id_];
  s.linear_acceleration = {{acc[0], acc[1], acc[2]}};
  return s;
}

BasePoseSnapshot MujocoEngine::base_pose() const
{
  BasePoseSnapshot pose;
  for (int i = 0; i < 3; ++i) {
    pose.position[i] = data_->qpos[base_qpos_adr_ + i];
  }
  for (int i = 0; i < 4; ++i) {
    pose.orientation_wxyz[i] = data_->qpos[base_qpos_adr_ + 3 + i];
  }
  return pose;
}

}  // namespace hhros2_sim
