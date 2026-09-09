#!/usr/bin/env bash
# Install ROS 2 system dependencies for yidong_robot workspace.
# Run from any directory after sourcing your ROS distro:
#   source /opt/ros/$ROS_DISTRO/setup.bash
#   bash ros2_ws/install_deps.sh

set -euo pipefail

if [[ -z "${ROS_DISTRO:-}" ]]; then
  echo "ERROR: ROS_DISTRO is not set. Source your ROS install first, e.g.:"
  echo "  source /opt/ros/humble/setup.bash"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS_DIR="$(cd "${SCRIPT_DIR}" && pwd)"

echo "==> Installing apt packages for ROS ${ROS_DISTRO}..."
sudo apt-get update
sudo apt-get install -y \
  "ros-${ROS_DISTRO}-nmea-msgs" \
  "ros-${ROS_DISTRO}-mavros-msgs" \
  "ros-${ROS_DISTRO}-ros2-control" \
  "ros-${ROS_DISTRO}-ros2-controllers" \
  "ros-${ROS_DISTRO}-controller-manager" \
  "ros-${ROS_DISTRO}-hardware-interface" \
  "ros-${ROS_DISTRO}-controller-interface" \
  "ros-${ROS_DISTRO}-joint-state-broadcaster" \
  "ros-${ROS_DISTRO}-imu-sensor-broadcaster" \
  "ros-${ROS_DISTRO}-xacro" \
  "ros-${ROS_DISTRO}-robot-state-publisher" \
  "ros-${ROS_DISTRO}-cv-bridge" \
  "ros-${ROS_DISTRO}-tf2-ros" \
  "ros-${ROS_DISTRO}-lifecycle-msgs" \
  "ros-${ROS_DISTRO}-diagnostic-msgs" \
  libboost-system-dev \
  libboost-thread-dev \
  libboost-program-options-dev \
  libyaml-cpp-dev \
  libeigen3-dev \
  libspdlog-dev \
  nlohmann-json3-dev

if command -v rosdep >/dev/null 2>&1; then
  echo "==> rosdep install (workspace src)..."
  rosdep update || true
  rosdep install --from-paths "${WS_DIR}/src" --ignore-src -r -y || true
else
  echo "WARN: rosdep not found; apt packages above should cover most deps."
fi

echo "==> Done. Build with:"
echo "  cd ${WS_DIR} && colcon build --symlink-install"
