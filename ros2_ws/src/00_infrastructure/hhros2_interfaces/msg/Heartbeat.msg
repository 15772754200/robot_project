# Lightweight liveness beat each managed node publishes to hhros2_core.
# Absence of beats within a deadline is itself a Level 2 fault condition.
std_msgs/Header header
string node_name
uint8 level                # node's self-reported SafetyStatus level
float64 loop_rate_hz       # measured loop rate (0 if event-driven)
