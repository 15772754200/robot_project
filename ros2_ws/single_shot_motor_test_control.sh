#!/usr/bin/env bash
set -eo pipefail

WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$WORKSPACE" || exit 1
source install/setup.bash
set -u

disable_motors()
{
    ros2 service call /hhros2_core/set_system_state \
        hhros2_interfaces/srv/SetSystemState "{enable: false}" \
        >/dev/null 2>&1 || true
}

trap disable_motors EXIT INT TERM

call_required_service()
{
    local response
    response="$(ros2 service call "$@")"
    printf '%s\n' "$response"
    if ! grep -Eq 'success[=:][[:space:]]*(true|True)' <<<"$response"; then
        echo "Refusing to continue because the service request was rejected."
        exit 1
    fi
}

echo "Checking the isolated diagnostic command topic..."
publisher_count="$(
    ros2 topic info /humanoid_base_controller/reference |
    awk '/Publisher count:/ { print $3 }'
)"
if [[ "$publisher_count" != "0" ]]; then
    echo "Refusing to start: expected 0 existing publishers on"
    echo "/humanoid_base_controller/reference, found ${publisher_count:-unknown}."
    exit 1
fi

read -r -p "Enable motors and run one 0.005 rad left hip pitch latency step? (y/N) " answer
if [[ ! "$answer" =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

call_required_service /hhros2_core/set_control_mode \
    hhros2_interfaces/srv/SetControlMode "{mode: 2}"

call_required_service /hhros2_core/set_system_state \
    hhros2_interfaces/srv/SetSystemState "{enable: true}"

ros2 run hhros2_motor_diagnostics single_shot_motor_probe \
    --ros-args \
    --params-file \
    "$WORKSPACE/install/hhros2_motor_diagnostics/share/hhros2_motor_diagnostics/config/single_shot_motor_probe.yaml"
