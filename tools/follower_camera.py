#!/usr/bin/env python3
"""SO-ARM101 从臂摄像头 FPV 接收节点 (ROS 2 Humble)。

读取 ESP32-NanoCam 板载摄像头的 MJPEG 流 (http://<NANOCAM_IP>:81/stream)，
逐帧解析 JPEG 并发布为 ROS 2 话题，可与 RViz / image_transport 配合使用。

话题:
  /follower_camera/image_raw/compressed  sensor_msgs/msg/CompressedImage  (best_effort)

用法:
  # 终端 1: 正常运行遥操作 (./start_soarm_demo.sh)
  # 终端 2:
  source /opt/ros/humble/setup.bash
  python3 tools/follower_camera.py --stream http://192.168.x.x:81/stream

  # 查看:
  ros2 topic hz /follower_camera/image_raw/compressed
  rviz2   # 添加 "Camera" 显示, topic 选 /follower_camera/image_raw/compressed

依赖: pip install opencv-python   (仅 --preview 预览时需要)
"""

from __future__ import annotations

import argparse
import re
import time
import urllib.request

import rclpy
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import CompressedImage


def iter_mjpeg_frames(stream_url: str):
    """解析 HTTP MJPEG (multipart/x-mixed-replace) 流，逐帧产出 JPEG 字节。"""
    stream = urllib.request.urlopen(stream_url, timeout=10)
    content_type = stream.headers.get("Content-Type", "")
    m = re.search(r"boundary=([^;\s]+)", content_type)
    if not m:
        raise RuntimeError(f"不是 MJPEG 流 (Content-Type: {content_type})")

    boundary = ("--" + m.group(1)).encode()
    buf = b""
    while True:
        chunk = stream.read(65536)
        if not chunk:
            break
        buf += chunk
        while True:
            start = buf.find(boundary)
            if start < 0:
                # 只保留尾部，防止边界跨块时丢失
                if len(buf) > 2 * 65536:
                    buf = buf[-65536:]
                break
            end = buf.find(boundary, start + len(boundary))
            if end < 0:
                break
            part = buf[start + len(boundary):end]
            buf = buf[end:]
            header_end = part.find(b"\r\n\r\n")
            if header_end < 0:
                continue
            yield part[header_end + 4:]


class CameraReceiver(Node):
    def __init__(self, stream_url: str, topic: str, max_fps: float):
        super().__init__("follower_camera")
        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=2,
        )
        self.pub = self.create_publisher(CompressedImage, topic, qos)
        self.stream_url = stream_url
        self.min_interval = 1.0 / max_fps if max_fps > 0 else 0.0
        self.get_logger().info(f"连接视频流: {stream_url}")

    def run(self) -> None:
        frames = 0
        last_log = time.time()
        last_pub = 0.0
        for jpeg in iter_mjpeg_frames(self.stream_url):
            now = time.time()
            if now - last_pub < self.min_interval:
                continue
            last_pub = now
            msg = CompressedImage()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = "follower_camera"
            msg.format = "jpeg"
            msg.data = bytes(jpeg)
            self.pub.publish(msg)
            frames += 1
            if now - last_log >= 5.0:
                self.get_logger().info(f"已发布 {frames} 帧")
                frames = 0
                last_log = now


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stream", required=True,
        help="MJPEG 流地址, 如 http://192.168.1.100:81/stream")
    parser.add_argument("--topic", default="/follower_camera/image_raw/compressed",
        help="ROS 2 发布话题 (默认: /follower_camera/image_raw/compressed)")
    parser.add_argument("--max-fps", type=float, default=0.0,
        help="限制发布帧率, 0 表示不限 (默认: 0)")
    args = parser.parse_args()

    rclpy.init()
    node = CameraReceiver(args.stream, args.topic, args.max_fps)
    try:
        node.run()
    except Exception as exc:  # noqa: BLE001
        node.get_logger().error(f"视频流中断: {exc}")
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
