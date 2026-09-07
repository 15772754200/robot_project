# Arbitrated control mode emitted by the hhros2_core mode arbiter. This is the
# single authority that decides which ros2_control controller is active, so the
# behavior layer and the safety reflex cannot fight over the actuators.
std_msgs/Header header

uint8 MODE_PASSIVE=0     # no controller active, joints released
uint8 MODE_DAMPING=1     # damping_controller (safe reflex)
uint8 MODE_STAND=2       # safe standing pose
uint8 MODE_WALK=3        # RL walk
uint8 MODE_RUN=4         # RL run
uint8 MODE_WBC=5         # WBC/QP whole-body task
uint8 MODE_MOTION=6      # scripted / CSV motion playback

uint8 active_mode
string active_controller # ros2_control controller name currently claimed
string source            # requesting layer: "behavior" | "core" | "teleop"
