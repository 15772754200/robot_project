#ifndef HHROS2_MOTION_CORES_RL_MATH_UTILS_HPP_
#define HHROS2_MOTION_CORES_RL_MATH_UTILS_HPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace hhros2_motion_cores::rl_policy_math_utils {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

/// @brief Normalize an angle to the range [-π, π].
template <typename T>
inline T NormalizeAngle(T angle)
{
    return std::remainder(angle, static_cast<T>(kTwoPi));
}

/// @brief Convert quaternion (x, y, z, w) to Roll-Pitch-Yaw.
///
/// Rotation order:
///     Roll  -> X axis
///     Pitch -> Y axis
///     Yaw   -> Z axis
///
/// @param x Quaternion x component.
/// @param y Quaternion y component.
/// @param z Quaternion z component.
/// @param w Quaternion w component.
/// @return {roll, pitch, yaw} in radians.
template <typename T>
inline std::array<T, 3> QuaternionToRPY(T x, T y, T z, T w)
{
    // Roll (X-axis rotation)
    const T sinr_cosp = static_cast<T>(2) * (w * x + y * z);
    const T cosr_cosp =
        static_cast<T>(1) -
        static_cast<T>(2) * (x * x + y * y);
    const T roll = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (Y-axis rotation)
    T sinp = static_cast<T>(2) * (w * y - z * x);
    sinp = std::clamp(sinp, static_cast<T>(-1), static_cast<T>(1));
    const T pitch = std::asin(sinp);

    // Yaw (Z-axis rotation)
    const T siny_cosp = static_cast<T>(2) * (w * z + x * y);
    const T cosy_cosp =
        static_cast<T>(1) -
        static_cast<T>(2) * (y * y + z * z);
    const T yaw = std::atan2(siny_cosp, cosy_cosp);

    return {
        NormalizeAngle(roll),
        NormalizeAngle(pitch),
        NormalizeAngle(yaw)};
}

/// @brief Convert quaternion stored in std::array to Roll-Pitch-Yaw.
///
/// Quaternion order:
///     [x, y, z, w]
template <typename T>
inline std::array<T, 3> QuaternionToRPY(
    const std::array<T, 4>& quaternion)
{
    return QuaternionToRPY(
        quaternion[0],
        quaternion[1],
        quaternion[2],
        quaternion[3]);
}

/// @brief Convert quaternion stored in std::vector to Roll-Pitch-Yaw.
///
/// Quaternion order:
///     [x, y, z, w]
///
/// @return {0,0,0} if the input size is invalid.
template <typename T>
inline std::array<T, 3> QuaternionToRPY(
    const std::vector<T>& quaternion)
{
    if (quaternion.size() != 4) {
        return {T(0), T(0), T(0)};
    }

    return QuaternionToRPY(
        quaternion[0],
        quaternion[1],
        quaternion[2],
        quaternion[3]);
}

/// @brief Rotate a vector from world frame to body frame using the inverse
/// quaternion.
///
/// Quaternion order:
///     [x, y, z, w]
///
/// @param quaternion Quaternion (x, y, z, w).
/// @param vec Vector expressed in the world frame.
/// @return Rotated vector expressed in the body frame.
template <typename T>
inline std::array<T, 3> QuatRotateInverse(
    const std::array<T, 4>& quaternion,
    const std::array<T, 3>& vec)
{
    const T qx = quaternion[0];
    const T qy = quaternion[1];
    const T qz = quaternion[2];
    const T qw = quaternion[3];

    const T vx = vec[0];
    const T vy = vec[1];
    const T vz = vec[2];

    const T two = static_cast<T>(2);

    const T q_dot_v =
        qx * vx +
        qy * vy +
        qz * vz;

    const T cross_x = qy * vz - qz * vy;
    const T cross_y = qz * vx - qx * vz;
    const T cross_z = qx * vy - qy * vx;

    const T a = two * qw * qw - static_cast<T>(1);

    return {
        vx * a - two * qw * cross_x + two * qx * q_dot_v,
        vy * a - two * qw * cross_y + two * qy * q_dot_v,
        vz * a - two * qw * cross_z + two * qz * q_dot_v};
}

/// @brief Rotate a vector from world frame to body frame using the inverse
/// quaternion.
///
/// Quaternion order:
///     [x, y, z, w]
template <typename T>
inline std::array<T, 3> QuatRotateInverse(
    const std::vector<T>& quaternion,
    const std::array<T, 3>& vec)
{
    if (quaternion.size() != 4) {
        return vec;
    }

    return QuatRotateInverse(
        std::array<T, 4>{
            quaternion[0],
            quaternion[1],
            quaternion[2],
            quaternion[3]},
        vec);
}

}  // namespace hhros2_motion_cores::rl_policy_math_utils

#endif  // HHROS2_MOTION_CORES_RL_POLICY_MATH_UTILS_HPP_