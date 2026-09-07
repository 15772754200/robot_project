#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

source install/setup.bash

ros2 launch robot_embeded_bringup robot_embeded.launch.py &
LAUNCH_PID=$!

cleanup() {
  if kill -0 "$LAUNCH_PID" 2>/dev/null; then
    kill "$LAUNCH_PID"
    wait "$LAUNCH_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

ros2 run robot_embeded_bringup customer_panel
