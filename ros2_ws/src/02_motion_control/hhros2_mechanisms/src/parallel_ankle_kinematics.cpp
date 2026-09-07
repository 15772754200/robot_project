#include "hhros2_mechanisms/parallel_ankle_kinematics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace hhros2_mechanisms {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInverseL1 = 0.0396;
constexpr double kInverseD1 = 0.021;
constexpr double kH1 = 0.18;
constexpr double kH2 = 0.15;
constexpr double kJacobianL1 = 0.038;
constexpr double kJacobianD1 = 0.022;
constexpr double kAnkleJointLimitRad = 0.7854;
constexpr double kAnkleJointLimitDeg = kAnkleJointLimitRad * 180.0 / kPi;
constexpr double kForwardSolveMaxCostDeg2 = 1e-4;

double AxisSign(double value)
{
  return value >= 0.0 ? 1.0 : -1.0;
}

}  // namespace

ParallelAnkleKinematics::ParallelAnkleKinematics()
{
  right_config_ = AxisConfig{-1.0, -1.0, -1.0, 1.0};
  left_config_ = AxisConfig{-1.0, -1.0, 1.0, 1.0};
}

double ParallelAnkleKinematics::RadToDeg(double radians)
{
  return radians * 180.0 / kPi;
}

double ParallelAnkleKinematics::DegToRad(double degrees)
{
  return degrees * kPi / 180.0;
}

double ParallelAnkleKinematics::InverseKinematics::Solve1D(
    const std::function<double(double)>& function, double x0)
{
  constexpr double kEpsilon = 1e-9;
  constexpr double kStep = 1e-6;
  double x = x0;

  for (int i = 0; i < 30; ++i) {
    const double fx = function(x);
    if (std::abs(fx) < kEpsilon) {
      break;
    }

    const double derivative =
        (function(x + kStep) - function(x - kStep)) / (2.0 * kStep);
    if (std::abs(derivative) < 1e-12) {
      break;
    }

    const double delta = fx / derivative;
    x -= delta;
    if (std::abs(delta) < 1e-10) {
      break;
    }
  }

  return x;
}

std::pair<double, double> ParallelAnkleKinematics::InverseKinematics::SolveRight(
    double pitch_deg,
    double roll_deg,
    const AxisConfig& config) const
{
  const double motor1_axis = AxisSign(config.motor1_axis);
  const double motor2_axis = AxisSign(config.motor2_axis);
  const double pitch_axis = AxisSign(config.pitch_axis);
  const double roll_axis = AxisSign(config.roll_axis);

  const double pitch = DegToRad(pitch_deg) * pitch_axis;
  const double roll = DegToRad(roll_deg) * roll_axis;

  const Eigen::Vector3d d(
      -kInverseL1 * std::cos(pitch),
      -kInverseD1 * std::cos(roll),
      kInverseL1 * std::sin(pitch) - kInverseD1 * std::sin(roll));
  const Eigen::Vector3d c(
      -kInverseL1 * std::cos(pitch),
      kInverseD1 * std::cos(roll),
      kInverseL1 * std::sin(pitch) + kInverseD1 * std::sin(roll));

  auto motor1_error = [&](double motor_raw) {
    const double motor = motor_raw * motor1_axis;
    const Eigen::Vector3d f(
        -kInverseL1,
        -kInverseD1 * std::cos(motor),
        kH1 - kInverseD1 * std::sin(motor));
    return (f - d).squaredNorm() - kH1 * kH1;
  };

  auto motor2_error = [&](double motor_raw) {
    const double motor = motor_raw * motor2_axis;
    const Eigen::Vector3d e(
        -kInverseL1,
        kInverseD1 * std::cos(motor),
        kH2 + kInverseD1 * std::sin(motor));
    return (e - c).squaredNorm() - kH2 * kH2;
  };

  return {
      RadToDeg(Solve1D(motor1_error, 0.0)),
      RadToDeg(Solve1D(motor2_error, 0.0))};
}

std::pair<double, double> ParallelAnkleKinematics::InverseKinematics::SolveLeft(
    double pitch_deg,
    double roll_deg,
    const AxisConfig& config) const
{
  const double motor1_axis = AxisSign(config.motor1_axis);
  const double motor2_axis = AxisSign(config.motor2_axis);
  const double pitch_axis = AxisSign(config.pitch_axis);
  const double roll_axis = AxisSign(config.roll_axis);

  const double pitch = DegToRad(pitch_deg) * pitch_axis;
  const double roll = DegToRad(roll_deg) * roll_axis;

  const Eigen::Vector3d d(
      -kInverseL1 * std::cos(pitch),
      -kInverseD1 * std::cos(roll),
      kInverseL1 * std::sin(pitch) - kInverseD1 * std::sin(roll));
  const Eigen::Vector3d c(
      -kInverseL1 * std::cos(pitch),
      kInverseD1 * std::cos(roll),
      kInverseL1 * std::sin(pitch) + kInverseD1 * std::sin(roll));

  auto motor1_error = [&](double motor_raw) {
    const double motor = motor_raw * motor1_axis;
    const Eigen::Vector3d f(
        -kInverseL1,
        -kInverseD1 * std::cos(motor),
        kH2 - kInverseD1 * std::sin(motor));
    return (f - d).squaredNorm() - kH2 * kH2;
  };

  auto motor2_error = [&](double motor_raw) {
    const double motor = motor_raw * motor2_axis;
    const Eigen::Vector3d e(
        -kInverseL1,
        kInverseD1 * std::cos(motor),
        kH1 + kInverseD1 * std::sin(motor));
    return (e - c).squaredNorm() - kH1 * kH1;
  };

  const double motor1 = Solve1D(motor1_error, 0.0);
  const double motor2 = Solve1D(motor2_error, 0.0);
  return {RadToDeg(motor2), RadToDeg(motor1)};
}

ParallelAnkleKinematics::ForwardFunctor::ForwardFunctor(
    const InverseKinematics& solver_in,
    const AxisConfig& config_in,
    FootSide side_in,
    double target_motor1_deg_in,
    double target_motor2_deg_in)
: solver(solver_in),
  config(config_in),
  side(side_in),
  target_motor1_deg(target_motor1_deg_in),
  target_motor2_deg(target_motor2_deg_in)
{
}

int ParallelAnkleKinematics::ForwardFunctor::operator()(
    const InputType& x, ValueType& fvec) const
{
  const auto predicted =
      (side == FootSide::kRight)
          ? solver.SolveRight(x[0], x[1], config)
          : solver.SolveLeft(x[0], x[1], config);
  fvec[0] = predicted.first - target_motor1_deg;
  fvec[1] = predicted.second - target_motor2_deg;
  return 0;
}

int ParallelAnkleKinematics::ForwardFunctor::df(
    const InputType& x, JacobianType& fjac) const
{
  ValueType f0(2);
  operator()(x, f0);

  constexpr double kEpsilon = 1e-6;
  for (int i = 0; i < 2; ++i) {
    InputType x_eps = x;
    x_eps[i] += kEpsilon;
    ValueType f_eps(2);
    operator()(x_eps, f_eps);
    fjac.col(i) = (f_eps - f0) / kEpsilon;
  }
  return 0;
}

ParallelAnkleForwardResult ParallelAnkleKinematics::ForwardKinematics(
    double motor1_deg,
    double motor2_deg,
    const AxisConfig& config,
    FootSide side,
    const Eigen::Vector2d& initial_deg)
{
  InverseKinematics solver;
  const Eigen::Vector2d bounded_initial(
      std::clamp(initial_deg[0], -kAnkleJointLimitDeg, kAnkleJointLimitDeg),
      std::clamp(initial_deg[1], -kAnkleJointLimitDeg, kAnkleJointLimitDeg));
  std::vector<Eigen::Vector2d> starts{bounded_initial};
  const std::vector<double> offsets{-2.0, 0.0, 2.0};
  for (const double pitch_offset : offsets) {
    for (const double roll_offset : offsets) {
      const Eigen::Vector2d start(
          std::clamp(
              bounded_initial[0] + pitch_offset,
              -kAnkleJointLimitDeg,
              kAnkleJointLimitDeg),
          std::clamp(
              bounded_initial[1] + roll_offset,
              -kAnkleJointLimitDeg,
              kAnkleJointLimitDeg));
      bool duplicate = false;
      for (const auto& existing : starts) {
        if ((existing - start).norm() < 1e-12) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        starts.push_back(start);
      }
    }
  }

  ParallelAnkleForwardResult best;
  best.cost = std::numeric_limits<double>::infinity();

  for (const auto& start : starts) {
    ForwardFunctor functor(solver, config, side, motor1_deg, motor2_deg);
    Eigen::LevenbergMarquardt<ForwardFunctor> lm(functor);
    lm.parameters.maxfev = 40;
    lm.parameters.ftol = 1e-10;
    lm.parameters.xtol = 1e-10;

    Eigen::VectorXd x(2);
    x = start;
    const auto status = lm.minimize(x);

    Eigen::VectorXd fvec(2);
    functor(x, fvec);
    const double cost = fvec.squaredNorm();
    if (!x.allFinite() || !std::isfinite(cost) ||
        std::abs(x[0]) > kAnkleJointLimitDeg ||
        std::abs(x[1]) > kAnkleJointLimitDeg) {
      continue;
    }
    if (cost < best.cost) {
      best.cost = cost;
      best.pitch_rad = DegToRad(x[0]);
      best.roll_rad = DegToRad(x[1]);
      best.status = status;
      if (cost < 1e-8) {
        break;
      }
    }
  }

  return best;
}

std::pair<double, double> ParallelAnkleKinematics::InverseSolveRad(
    double pitch_rad, double roll_rad, FootSide side) const
{
  const double pitch_deg = RadToDeg(pitch_rad);
  const double roll_deg = RadToDeg(roll_rad);
  const auto result =
      (side == FootSide::kRight)
          ? inverse_.SolveRight(pitch_deg, roll_deg, right_config_)
          : inverse_.SolveLeft(pitch_deg, roll_deg, left_config_);
  return {DegToRad(result.first), DegToRad(result.second)};
}

ParallelAnkleForwardResult ParallelAnkleKinematics::ForwardSolveRad(
    double motor1_rad,
    double motor2_rad,
    FootSide side,
    const Eigen::Vector2d& initial_deg) const
{
  const AxisConfig& config = (side == FootSide::kRight) ? right_config_ : left_config_;
  return ForwardKinematics(
      RadToDeg(motor1_rad), RadToDeg(motor2_rad), config, side, initial_deg);
}

Eigen::Vector2d ParallelAnkleKinematics::ImplicitJacobianRowRad(
    double pitch_rad,
    double roll_rad,
    double motor_raw_rad,
    double pitch_axis,
    double roll_axis,
    double motor_axis,
    double y_sign,
    double z_sign,
    double rod_height)
{
  const double pitch = pitch_rad * AxisSign(pitch_axis);
  const double roll = roll_rad * AxisSign(roll_axis);
  const double motor = motor_raw_rad * AxisSign(motor_axis);

  const double dx = kJacobianL1 * (std::cos(pitch) - 1.0);
  const double dy = y_sign * kJacobianD1 * (std::cos(motor) - std::cos(roll));
  const double dz =
      rod_height - kJacobianL1 * std::sin(pitch) +
      z_sign * kJacobianD1 * (std::sin(motor) - std::sin(roll));

  const double f_pitch =
      -2.0 * kJacobianL1 * (dx * std::sin(pitch) + dz * std::cos(pitch));
  const double f_roll =
      2.0 * kJacobianD1 *
      (y_sign * dy * std::sin(roll) - z_sign * dz * std::cos(roll));
  const double f_motor =
      2.0 * kJacobianD1 *
      (-y_sign * dy * std::sin(motor) + z_sign * dz * std::cos(motor));

  const double df_dpitch = f_pitch * AxisSign(pitch_axis);
  const double df_droll = f_roll * AxisSign(roll_axis);
  const double df_dmotor = f_motor * AxisSign(motor_axis);

  if (std::abs(df_dmotor) < 1e-12) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return Eigen::Vector2d(nan, nan);
  }

  return Eigen::Vector2d(-df_dpitch / df_dmotor, -df_droll / df_dmotor);
}

Eigen::Matrix2d ParallelAnkleKinematics::JointToMotorJacobianAnalyticalRad(
    double pitch_rad, double roll_rad, FootSide side) const
{
  const AxisConfig& config = (side == FootSide::kRight) ? right_config_ : left_config_;
  const auto motors = InverseSolveRad(pitch_rad, roll_rad, side);

  Eigen::Matrix2d jacobian = Eigen::Matrix2d::Zero();
  if (side == FootSide::kRight) {
    jacobian.row(0) =
        ImplicitJacobianRowRad(
            pitch_rad, roll_rad, motors.first, config.pitch_axis,
            config.roll_axis, config.motor1_axis, -1.0, -1.0, kH1)
            .transpose();
    jacobian.row(1) =
        ImplicitJacobianRowRad(
            pitch_rad, roll_rad, motors.second, config.pitch_axis,
            config.roll_axis, config.motor2_axis, 1.0, 1.0, kH2)
            .transpose();
  } else {
    jacobian.row(0) =
        ImplicitJacobianRowRad(
            pitch_rad, roll_rad, motors.first, config.pitch_axis,
            config.roll_axis, config.motor2_axis, 1.0, 1.0, kH1)
            .transpose();
    jacobian.row(1) =
        ImplicitJacobianRowRad(
            pitch_rad, roll_rad, motors.second, config.pitch_axis,
            config.roll_axis, config.motor1_axis, -1.0, -1.0, kH2)
            .transpose();
  }
  return jacobian;
}

Eigen::Matrix2d ParallelAnkleKinematics::JointToMotorJacobianNumericalRad(
    double pitch_rad, double roll_rad, FootSide side) const
{
  constexpr double kEpsilon = 1e-4;
  auto evaluate = [this, side](double pitch, double roll) {
    const auto motors = InverseSolveRad(pitch, roll, side);
    return Eigen::Vector2d(motors.first, motors.second);
  };

  Eigen::Matrix2d jacobian;
  jacobian.col(0) =
      (evaluate(pitch_rad + kEpsilon, roll_rad) -
       evaluate(pitch_rad - kEpsilon, roll_rad)) /
      (2.0 * kEpsilon);
  jacobian.col(1) =
      (evaluate(pitch_rad, roll_rad + kEpsilon) -
       evaluate(pitch_rad, roll_rad - kEpsilon)) /
      (2.0 * kEpsilon);
  return jacobian;
}

Eigen::Matrix2d ParallelAnkleKinematics::JointToMotorJacobianRad(
    double pitch_rad, double roll_rad, FootSide side) const
{
  const Eigen::Matrix2d analytical =
      JointToMotorJacobianAnalyticalRad(pitch_rad, roll_rad, side);
  if (analytical.allFinite()) {
    return analytical;
  }
  return JointToMotorJacobianNumericalRad(pitch_rad, roll_rad, side);
}

Eigen::Matrix2d ParallelAnkleKinematics::StableInverse(
    const Eigen::Matrix2d& jacobian)
{
  constexpr double kEpsilon = 1e-8;
  if (std::abs(jacobian.determinant()) > kEpsilon) {
    return jacobian.inverse();
  }

  const Eigen::JacobiSVD<Eigen::Matrix2d> svd(
      jacobian, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix2d singular_inv = Eigen::Matrix2d::Zero();
  for (int i = 0; i < 2; ++i) {
    const double sigma = svd.singularValues()[i];
    if (sigma > kEpsilon) {
      singular_inv(i, i) = 1.0 / sigma;
    }
  }
  return svd.matrixV() * singular_inv * svd.matrixU().transpose();
}

void ParallelAnkleKinematics::JointToMotor(
    Eigen::Vector2d& q,
    Eigen::Vector2d& velocity,
    FootSide side) const
{
  const double pitch_rad = q[0];
  const double roll_rad = q[1];
  const auto motors = InverseSolveRad(pitch_rad, roll_rad, side);
  const Eigen::Matrix2d joint_to_motor =
      JointToMotorJacobianRad(pitch_rad, roll_rad, side);

  q << motors.first, motors.second;
  velocity = joint_to_motor * velocity;
}

bool ParallelAnkleKinematics::MotorToJoint(
    Eigen::Vector2d& q,
    Eigen::Vector2d& velocity,
    FootSide side,
    const Eigen::Vector2d& initial_deg) const
{
  const ParallelAnkleForwardResult forward =
      ForwardSolveRad(q[0], q[1], side, initial_deg);
  if (!std::isfinite(forward.cost) ||
      forward.cost > kForwardSolveMaxCostDeg2 ||
      !std::isfinite(forward.pitch_rad) ||
      !std::isfinite(forward.roll_rad) ||
      std::abs(forward.pitch_rad) > kAnkleJointLimitRad ||
      std::abs(forward.roll_rad) > kAnkleJointLimitRad) {
    return false;
  }

  const Eigen::Matrix2d joint_to_motor =
      JointToMotorJacobianRad(forward.pitch_rad, forward.roll_rad, side);
  const Eigen::Vector2d joint_velocity =
      StableInverse(joint_to_motor) * velocity;
  if (!joint_velocity.allFinite()) {
    return false;
  }

  q << forward.pitch_rad, forward.roll_rad;
  velocity = joint_velocity;
  return true;
}

}  // namespace hhros2_mechanisms
