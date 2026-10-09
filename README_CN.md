[English](README.md) | 简体中文

# SO-ARM101 无线遥操作（ESP32-NanoCam 版）

面向比赛演示的无人机搭载 SO-ARM101 机械臂无线遥操作项目：
主臂通过 LeRobot 连接 Ubuntu 电脑，从臂使用自研 **ESP32-NanoCam 模块**
（ESP32-S3 N16R8），通过 micro-ROS WiFi UDP 接收控制，并集成板载摄像头FPV、麦克风、扬声器与 RGB 状态灯。

```
SO-ARM101 主臂 → USB 舵机驱动板 → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                      │  2.4 GHz Wi-Fi
                                      ▼
                        ESP32-NanoCam (从臂控制器, P2-7/P2-8 接舵机总线)
                                      │  1 Mbps UART 总线
                                      ▼
                        SO-ARM101 从臂 (6 × STS3215)
```

## 目录结构

```text
firmware/nanocam_soarm/   ESP32-NanoCam 从臂固件 (PlatformIO)
  ├─ boards/nano_cam.json 自研板卡定义 (16MB Flash / 8MB Octal PSRAM)
  ├─ src/                 固件源码 (micro-ROS 遥操作 + 摄像头 + 音频 + RGB)
  ├─ lib/microros/        micro-ROS 静态库 (xtensa-lx7)
  └─ lib/scservo/         SCServo 舵机库 (本地化, 无网络依赖)
tools/                    PC 端脚本 (wireless_teleoperate.py 遥操作桥, follower_camera.py FPV 接收)
start_soarm_demo.sh       一键启动脚本 (网络/Agent/标定预检 + 遥操作)
cali/                     主臂/从臂标定文件
docs/                     项目进度与实验记录 + 硬件参考 (docs/reference/)
```

## 功能

- **micro-ROS 无线遥操作**：`/joint_states` 20Hz 反馈、`/joint_command` 命令，完整安全机制（标定检查、姿态握手、软限位、步长限制、看门狗）
- **摄像头 FPV**：MJPEG 流 `http://<IP>/stream`（GC2145 QVGA），PC 端发布为 `sensor_msgs/CompressedImage`
- **麦克风**：音量电平 → `/follower_audio/level`（Float32, 5Hz）
- **扬声器**：启动/就绪/解锁/错误提示音（ES8311）
- **RGB 状态灯**：启动红 → WiFi 橙 → micro-ROS 绿 → 解锁蓝（WS2812）

## 快速开始

```bash
# 1. 配置固件 WiFi (firmware/nanocam_soarm/src/wifi_config.h) 并烧录
cd firmware/nanocam_soarm
# 打开 src/wifi_config.example.h 并复制为 wifi_config.h，填入 SSID/密码/Ubuntu 电脑 IP
python3 -m platformio run --target upload

# 2. Ubuntu PC: 启动 micro-ROS Agent
snap run micro-ros-agent udp4 --port 8888

# 3. 遥操作 (另一终端, 主臂接 USB 舵机驱动板)
./start_soarm_demo.sh --check  # 预飞检查
./start_soarm_demo.sh

# 4. (可选) 摄像头 FPV
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
```

完整说明见 [README_NANOCAM.md](./README_NANOCAM.md)。

## 硬件要点

### 接线方式

ESP32-NanoCam 与从臂之间**通过舵机驱动板的 UART 针脚中转**：

```text
舵机驱动板 UART:  RX ←── 连 NanoCam TX (P2-8 / GPIO20)
                  TX ──→ 连 NanoCam RX (P2-7 / GPIO19)
                 GND ──→ 连 NanoCam GND
```

- **TX 连 RX、RX 连 TX**（交叉），GND 共地，1 Mbps 波特率；
- ESP32-NanoCam 的 UART1 走模块 **P2-7（GPIO19, RX）/ P2-8（GPIO20, TX）**。

### 供电

| 设备 | 供电方式 |
|---|---|
| ESP32-NanoCam | **USB 数据线供电**（CH340K 调试串口同时工作） |
| 从臂（6×STS3215） | **12V 5A** 外部电源 |
| 主臂（连接 Ubuntu 电脑） | **5V 6A** 外部电源 |

> ⚠️ USB 带不动 6 个舵机，从臂必须用 12V 5A 外部供电；ESP32 用 USB 数据线供电。

### 其他

- 摄像头 GC2145 DVP；音频 ES8311 (I2S 24kHz)；RGB WS2812 @ GPIO18
- 固件标定已与 `cali/follower_recal.json` 对齐；更换从臂后需重新标定并同步更新 `servo_bus.cpp` 中的三个数组

## 来源

本项目从臂控制器由 ESP32-S3 → 自研 ESP32-NanoCam 演进而来。
