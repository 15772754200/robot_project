from __future__ import annotations

# Behavior placeholder node (03_intelligence). A minimal task state machine that
# stands in for the production BehaviorTree.CPP tree and the VLM/LLM embodied
# interface. It maps high-level instructions to the core-arbitrated control mode
# service, the shared base velocity topic, and the optional gait contract.
import shlex

import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node

from geometry_msgs.msg import Twist
from hhros2_interfaces.action import SetGait
from hhros2_interfaces.msg import ArbitrationMode, Heartbeat, SafetyStatus
from hhros2_interfaces.srv import SetControlMode
from rclpy.qos import QoSPolicyKind
from rclpy.qos_overriding_options import QoSOverridingOptions
from std_msgs.msg import String


MODE_BY_NAME = {
    "passive": ArbitrationMode.MODE_PASSIVE,
    "damping": ArbitrationMode.MODE_DAMPING,
    "stand": ArbitrationMode.MODE_STAND,
    "walk": ArbitrationMode.MODE_WALK,
    "run": ArbitrationMode.MODE_RUN,
    "wbc": ArbitrationMode.MODE_WBC,
    "motion": ArbitrationMode.MODE_MOTION,
}

MODE_NAME_BY_VALUE = {value: name for name, value in MODE_BY_NAME.items()}
SAFE_FALLBACK_MODES = {
    ArbitrationMode.MODE_PASSIVE,
    ArbitrationMode.MODE_DAMPING,
}
MOBILE_MODES = {
    ArbitrationMode.MODE_WALK,
    ArbitrationMode.MODE_RUN,
}
MODE_ALIASES = {
    "stop": "stand",
    "halt": "stand",
    "hold": "stand",
}
ZERO_VELOCITY_COMMANDS = {"zero", "zero_vel", "stop_vel"}
QOS_OVERRIDES = QoSOverridingOptions(policy_kinds=(
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
))


class BehaviorNode(Node):
    def __init__(self) -> None:
        super().__init__("hhros2_behavior")
        self.declare_parameter("autostart_stand", False)    # 自动站立
        self.declare_parameter(
            "instruction_topic", "/hhros2_behavior/instruction")    # 目前，我认为这个没有什么用处
        self.declare_parameter("cmd_vel_topic", "/cmd_vel")
        self.declare_parameter(
            "set_control_mode_service", "/hhros2_core/set_control_mode")
        self.declare_parameter("set_gait_action", "/set_gait")
        self.declare_parameter("default_walk_vx", 0.2)
        self.declare_parameter("default_run_vx", 0.45)
        self.declare_parameter("warning_speed_scale", 0.3)

        instruction_topic = str(self.get_parameter("instruction_topic").value)
        cmd_vel_topic = str(self.get_parameter("cmd_vel_topic").value)
        set_control_mode_service = str(
            self.get_parameter("set_control_mode_service").value)
        set_gait_action = str(self.get_parameter("set_gait_action").value)  # 设置步态
        self._default_walk_vx = float(self.get_parameter("default_walk_vx").value)
        self._default_run_vx = float(self.get_parameter("default_run_vx").value)
        self._warning_speed_scale = float(
            self.get_parameter("warning_speed_scale").value)

        self._mode_cli = self.create_client(
            SetControlMode, set_control_mode_service)   # 这个是模式切换
        self._gait_cli = ActionClient(self, SetGait, set_gait_action)   # 客户端要发布步态指令的，比如跑步，跳跃，行走
        self._beat_pub = self.create_publisher( # 这个是在发心跳的，证明它还活着
            Heartbeat, "/hhros2/heartbeat", 10,
            qos_overriding_options=QOS_OVERRIDES)
        self._safety_sub = self.create_subscription(
            SafetyStatus, "/hhros2_core/safety_status", self._on_safety, 10,
            qos_overriding_options=QOS_OVERRIDES)    # 接收core发布的反馈
        self._cmd_vel_pub = self.create_publisher(
            Twist, cmd_vel_topic, 10,
            qos_overriding_options=QOS_OVERRIDES) # 创建一个速度的发布者
        self._instruction_sub = self.create_subscription(   # 创建一下这个的订阅
            String, instruction_topic, self._on_instruction_msg, 10,
            qos_overriding_options=QOS_OVERRIDES)

        self._safety_level = SafetyStatus.LEVEL_OK
        self._autostarted = False
        self._set_control_mode_service = set_control_mode_service
        self.create_timer(0.2, self._on_beat)

        if self._as_bool(self.get_parameter("autostart_stand").value):
            self.create_timer(0.5, self._autostart_once)
        self.get_logger().info(
            "hhros2_behavior up: instructions on %s, cmd_vel on %s" %
            (instruction_topic, cmd_vel_topic))

    # --- governance plumbing ------------------------------------------------- #
    def _on_safety(self, msg: SafetyStatus) -> None:
        self._safety_level = msg.level

    def _on_beat(self) -> None:
        beat = Heartbeat()
        beat.header.stamp = self.get_clock().now().to_msg()
        beat.node_name = "hhros2_behavior"
        beat.level = SafetyStatus.LEVEL_OK
        self._beat_pub.publish(beat)

    def _as_bool(self, value) -> bool:
        if isinstance(value, str):
            return value.lower() in {"1", "true", "yes", "on"}
        return bool(value)

    # --- L3 -> L2/L1 contracts ----------------------------------------------- #
    def request_mode(self, mode: int) -> bool:  # 上层到下层切换控制模式
        if self._safety_level >= SafetyStatus.LEVEL_CRITICAL and \
                mode not in SAFE_FALLBACK_MODES:
            self.get_logger().warn(
                "refusing %s while safety level is CRITICAL" %  # 大于等于严重故障，正常和轻微轻微故障
                self._mode_name(mode))
            return False
        if not self._mode_cli.service_is_ready():   # 确保服务是开启的
            self.get_logger().warn(
                "%s service not ready." % self._set_control_mode_service)
            return False
        req = SetControlMode.Request()  # 类型的请求
        req.mode = mode
        future = self._mode_cli.call_async(req) # 异步请求，不堵塞
        self.get_logger().info( \
        "It has send async server: f{mode}")
        future.add_done_callback(   # future完成之后自动执行
            lambda done: self._on_mode_response(mode, done))    # 为什么用了lambda是因为callback只能有一个参数，就让mode是外部变量了
        return True # 这个只是发送成功，至于真正成不成要看回调的

    def _on_mode_response(self, mode: int, future) -> None:
        try:
            response = future.result()  # 得到反馈结果
        except Exception as exc:
            self.get_logger().error(
                "mode request %s failed: %s" % (self._mode_name(mode), exc))
            return
        log = self.get_logger().info if response.success \
            else self.get_logger().warn
        log(
            "mode request %s -> %s: %s" %
            (self._mode_name(mode), self._mode_name(response.active_mode),
             response.message))

    def send_gait(
        self,
        gait_name: str,
        cmd_vel: Twist | None = None,
        duration: float = 0.0,
    ) -> None:
        if not self._gait_cli.wait_for_server(timeout_sec=0.1):
            self.get_logger().warn("set_gait action server not available.")
            return
        goal = SetGait.Goal()
        goal.gait_name = gait_name
        goal.cmd_vel = cmd_vel if cmd_vel is not None else Twist()
        goal.duration = duration
        self._gait_cli.send_goal_async(goal)

    def on_instruction(self, text: str) -> None:
        # VLM/LLM hook: production should replace this small grammar with a
        # structured task parser. Keep the output contract stable: request a
        # core-arbitrated mode and publish base velocity.
        try:
            tokens = shlex.split(text.lower())
        except ValueError as exc:
            self.get_logger().warn("invalid instruction %r: %s" % (text, exc))
            return
        if not tokens:
            return

        command = MODE_ALIASES.get(tokens[0], tokens[0])    # 目前是不管输入什么，最后都会变成stand，如果没有就还是原来传进去的键
        if command in ZERO_VELOCITY_COMMANDS:
            self._publish_cmd_vel(Twist())  # 只要命令在0速度指令里面，就会发布速度为0
            return

        mode = MODE_BY_NAME.get(command)
        if mode is None:
            self.get_logger().warn(
                "unknown instruction %r; expected stand/walk/run/stop/"
                "passive/damping/wbc/motion" % text)
            return

        if mode in MOBILE_MODES:
            default_vx = self._default_vx_for(command)
            cmd_vel = self._twist_from_tokens(tokens[1:], default_vx)
            if self.request_mode(mode):
                self._publish_cmd_vel(cmd_vel)
            return

        self._publish_cmd_vel(Twist())
        self.request_mode(mode)

    def _on_instruction_msg(self, msg: String) -> None:
        self.on_instruction(msg.data)

    def _twist_from_tokens(self, tokens: list[str], default_vx: float) -> Twist:
        msg = Twist()
        msg.linear.x = default_vx
        for token in tokens:
            key = "vx"
            value = token
            if "=" in token:
                key, value = token.split("=", 1)
            try:
                number = float(value)
            except ValueError:
                self.get_logger().warn(
                    "ignoring non-numeric velocity token %r" % token)
                continue
            if key in {"vx", "x", "linear.x"}:
                msg.linear.x = number
            elif key in {"vy", "y", "linear.y"}:
                msg.linear.y = number
            elif key in {"wz", "yaw", "angular.z"}:
                msg.angular.z = number
            else:
                self.get_logger().warn(
                    "ignoring unknown velocity field %r" % key)
        return self._apply_safety_speed_limit(msg)

    def _default_vx_for(self, command: str) -> float:
        if command == "run":
            return self._default_run_vx
        return self._default_walk_vx

    def _apply_safety_speed_limit(self, msg: Twist) -> Twist:
        if self._safety_level != SafetyStatus.LEVEL_WARNING:
            return msg
        msg.linear.x *= self._warning_speed_scale
        msg.linear.y *= self._warning_speed_scale
        msg.angular.z *= self._warning_speed_scale
        self.get_logger().warn(
            "safety WARNING active; scaling cmd_vel by %.2f" %
            self._warning_speed_scale)
        return msg

    def _publish_cmd_vel(self, msg: Twist) -> None:
        self._cmd_vel_pub.publish(msg)
        self.get_logger().info(
            "cmd_vel x=%.3f y=%.3f yaw=%.3f" %
            (msg.linear.x, msg.linear.y, msg.angular.z))

    def _mode_name(self, mode: int) -> str:
        return MODE_NAME_BY_VALUE.get(mode, "mode_%d" % mode)

    def _autostart_once(self) -> None:
        if self._autostarted:   
            return
        if self._safety_level == SafetyStatus.LEVEL_OK:
            self.request_mode(ArbitrationMode.MODE_STAND)
            self._autostarted = True


def main(args=None) -> None:
    rclpy.init(args=args)
    node = BehaviorNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
