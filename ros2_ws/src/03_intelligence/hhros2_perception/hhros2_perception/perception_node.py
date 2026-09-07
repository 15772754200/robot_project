# tmny edit
# Perception placeholder node (03_intelligence). Consumes a point cloud and emits
# a local occupancy grid for Nav2, plus a governance heartbeat. The processing is
# a stub - the structure and topic contracts are what matter for the architecture.
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSPolicyKind, qos_profile_sensor_data
from rclpy.qos_overriding_options import QoSOverridingOptions

from hhros2_interfaces.msg import Heartbeat, SafetyStatus
from nav_msgs.msg import OccupancyGrid
from sensor_msgs.msg import PointCloud2


QOS_OVERRIDES = QoSOverridingOptions(policy_kinds=(
    QoSPolicyKind.HISTORY,
    QoSPolicyKind.DEPTH,
    QoSPolicyKind.RELIABILITY,
    QoSPolicyKind.DURABILITY,
))


class PerceptionNode(Node):
    def __init__(self) -> None:
        super().__init__("hhros2_perception")
        self.declare_parameter("cloud_topic", "/points")
        self.declare_parameter("grid_topic", "/local_costmap/occupancy")
        self.declare_parameter("rate_hz", 10.0)

        cloud_topic = self.get_parameter("cloud_topic").value
        grid_topic = self.get_parameter("grid_topic").value
        rate = float(self.get_parameter("rate_hz").value)

        self._cloud_sub = self.create_subscription(
            PointCloud2, cloud_topic, self._on_cloud, qos_profile_sensor_data,
            qos_overriding_options=QOS_OVERRIDES)
        self._grid_pub = self.create_publisher(
            OccupancyGrid, grid_topic, 1,
            qos_overriding_options=QOS_OVERRIDES)
        self._beat_pub = self.create_publisher(
            Heartbeat, "/hhros2/heartbeat", 10,
            qos_overriding_options=QOS_OVERRIDES)

        self._last_cloud_stamp = None
        self.create_timer(1.0 / rate, self._on_timer)
        self.get_logger().info("hhros2_perception placeholder up.")

    def _on_cloud(self, msg: PointCloud2) -> None:
        # Production: voxel filter -> ground removal -> occupancy update.        # tmny edit
        self._last_cloud_stamp = msg.header.stamp

    def _on_timer(self) -> None:
        grid = OccupancyGrid()
        grid.header.stamp = self.get_clock().now().to_msg()
        grid.header.frame_id = "odom"
        self._grid_pub.publish(grid)

        beat = Heartbeat()
        beat.header.stamp = grid.header.stamp
        beat.node_name = "hhros2_perception"
        # Level 1 (minor) if no cloud yet -> degrade to blind/slow mode upstream.# tmny edit
        beat.level = SafetyStatus.LEVEL_WARNING if self._last_cloud_stamp is None \
            else SafetyStatus.LEVEL_OK
        self._beat_pub.publish(beat)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = PerceptionNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
