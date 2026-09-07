# Aggregated safety verdict published by hhros2_core (one-horizontal governance).
#
# Fault levels mirror the commercial E-Stop tiers:
#   LEVEL_OK       - nominal operation
#   LEVEL_WARNING  - L1 minor (e.g. vision frame drop): degrade high-level speed
#   LEVEL_CRITICAL - L2 severe (loop latency > 3 cycles, fall-tilt exceeded):
#                    trigger safe reflex (switch to damping_controller / cut L0 enable)
std_msgs/Header header

uint8 LEVEL_OK=0
uint8 LEVEL_WARNING=1
uint8 LEVEL_CRITICAL=2

uint8 level
string source            # subsystem that raised the highest fault
string reason            # human-readable cause
bool safe_reflex_active  # critical fault latched; damping/cut handling is active
