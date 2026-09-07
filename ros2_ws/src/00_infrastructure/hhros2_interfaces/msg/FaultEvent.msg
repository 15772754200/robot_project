# A single fault report raised by any layer toward hhros2_core.
# The arbiter classifies and latches these into the aggregated SafetyStatus.
std_msgs/Header header

string source            # reporting node / subsystem, e.g. "perception", "control_loop", "imu"

uint8 LEVEL_INFO=0
uint8 LEVEL_DEGRADED=1   # Level 1 (minor): e.g. vision frame drop
uint8 LEVEL_CRITICAL=2   # Level 2 (severe): e.g. loop overrun, tilt over threshold
uint8 level

string code              # short machine-readable identifier, e.g. "LOOP_OVERRUN"
string description       # human-readable detail
float64 value            # optional measured value (latency ms, tilt rad, ...)
