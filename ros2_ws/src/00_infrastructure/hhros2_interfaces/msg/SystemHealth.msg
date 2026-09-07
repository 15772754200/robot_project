# Per-subsystem health snapshot aggregated by hhros2_core from node heartbeats.
# The three arrays are parallel (index i refers to the same subsystem).
std_msgs/Header header
string[] subsystem               # e.g. "hal", "estimation", "controllers", "perception"
uint8[] level                    # SafetyStatus level per subsystem
float64[] last_heartbeat_age_s   # seconds since last heartbeat (staleness)
uint8 aggregate_level            # worst level across all subsystems
