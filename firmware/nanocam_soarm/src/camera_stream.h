#pragma once

// ESP32-NanoCam 板载 DVP 摄像头（GC2145，兼容 OV2640/OV5640）MJPEG 流媒体服务。
// 提供 HTTP MJPEG 流（端口 81）：
//   http://<NANOCAM_IP>:81/         信息页
//   http://<NANOCAM_IP>:81/jpg      单帧 JPEG
//   http://<NANOCAM_IP>:81/stream   连续 MJPEG 流（FPV 用，PC 端 OpenCV/浏览器直接打开）
//
// 摄像头是可选的：没有接 Sense 摄像头时 start() 返回 false，
// 固件的 micro-ROS 遥操作功能不受影响。

namespace camera_stream {

// 启动摄像头 + HTTP 流媒体服务器。成功返回 true。
// 该服务运行在自己的 httpd 任务中，与主循环的 micro-ROS 遥操作并发。
bool start();

}  // namespace camera_stream
