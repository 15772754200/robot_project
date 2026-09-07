#pragma once

#include <functional>
#include <utility>

#include <Eigen/Dense>
#include <unsupported/Eigen/NonLinearOptimization>

namespace hhros2_mechanisms
{

enum class FootSide
{
    kLeft,
    kRight,
};

struct ParallelAnkleForwardResult
{
    double pitch_rad{0.0};
    double roll_rad{0.0};
    double cost{0.0};
    Eigen::LevenbergMarquardtSpace::Status status{
        Eigen::LevenbergMarquardtSpace::Status::NotStarted};
};

struct ParallelAnkleJointState
{
    double pitch_rad{0.0};
    double roll_rad{0.0};
    double pitch_vel{0.0};
    double roll_vel{0.0};
};

class ParallelAnkleKinematics
{
public:
    ParallelAnkleKinematics();

    std::pair<double, double> InverseSolveRad(
        double pitch_rad, double roll_rad, FootSide side) const;

    ParallelAnkleForwardResult ForwardSolveRad(
        double motor1_rad,
        double motor2_rad,
        FootSide side,
        const Eigen::Vector2d & initial_deg) const;

    Eigen::Matrix2d JointToMotorJacobianRad(
        double pitch_rad, double roll_rad, FootSide side) const;

    void JointToMotor(
        Eigen::Vector2d & q,
        Eigen::Vector2d & velocity,
        FootSide side) const;

    // Returns false when forward kinematics cannot find a physically valid
    // ankle configuration. On failure q and velocity are left unchanged.
    bool MotorToJoint(
        Eigen::Vector2d & q,
        Eigen::Vector2d & velocity,
        FootSide side,
        const Eigen::Vector2d & initial_deg) const;

private:
    struct AxisConfig
    {
        double motor1_axis{-1.0};
        double motor2_axis{-1.0};
        double pitch_axis{-1.0};
        double roll_axis{1.0};
    };

    class InverseKinematics
    {
    public:
        std::pair<double, double> SolveRight(
            double pitch_deg,
            double roll_deg,
            const AxisConfig & config) const;
        std::pair<double, double> SolveLeft(
            double pitch_deg,
            double roll_deg,
            const AxisConfig & config) const;

    private:
        static double Solve1D(
            const std::function<double(double)> & function, double x0);
    };

    struct ForwardFunctor
    {
        using Scalar = double;
        enum { InputsAtCompileTime = 2, ValuesAtCompileTime = 2 };
        using InputType = Eigen::VectorXd;
        using ValueType = Eigen::VectorXd;
        using JacobianType = Eigen::MatrixXd;

        ForwardFunctor(
            const InverseKinematics & solver,
            const AxisConfig & config,
            FootSide side,
            double target_motor1_deg,
            double target_motor2_deg);

        int operator()(const InputType & x, ValueType & fvec) const;
        int df(const InputType & x, JacobianType & fjac) const;
        int inputs() const { return 2; }
        int values() const { return 2; }

        const InverseKinematics & solver;
        AxisConfig config;
        FootSide side;
        double target_motor1_deg;
        double target_motor2_deg;
    };

    static ParallelAnkleForwardResult ForwardKinematics(
        double motor1_deg,
        double motor2_deg,
        const AxisConfig & config,
        FootSide side,
        const Eigen::Vector2d & initial_deg);

    Eigen::Matrix2d JointToMotorJacobianAnalyticalRad(
        double pitch_rad, double roll_rad, FootSide side) const;
    Eigen::Matrix2d JointToMotorJacobianNumericalRad(
        double pitch_rad, double roll_rad, FootSide side) const;

    static Eigen::Vector2d ImplicitJacobianRowRad(
        double pitch_rad,
        double roll_rad,
        double motor_raw_rad,
        double pitch_axis,
        double roll_axis,
        double motor_axis,
        double y_sign,
        double z_sign,
        double rod_height);

    static Eigen::Matrix2d StableInverse(const Eigen::Matrix2d & jacobian);
    static double RadToDeg(double radians);
    static double DegToRad(double degrees);

    InverseKinematics inverse_;
    AxisConfig right_config_;
    AxisConfig left_config_;
};

}  // namespace hhros2_mechanisms
