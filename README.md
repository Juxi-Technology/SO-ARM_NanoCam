English | [简体中文](README_CN.md)

# SO-ARM101 Wireless Teleoperation (ESP32-NanoCam Edition)

A wireless teleoperation project for a drone-mounted SO-ARM101 robotic arm, targeted at competition demos:
the leader arm connects to an Ubuntu computer via LeRobot, while the follower arm uses our in-house **ESP32-NanoCam module**
(ESP32-S3 N16R8), receives control over micro-ROS WiFi UDP, and integrates an onboard camera FPV, microphone, speaker and RGB status LED.

```
SO-ARM101 leader arm → USB servo driver board → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                      │  2.4 GHz Wi-Fi
                                      ▼
                        ESP32-NanoCam (follower controller, P2-7/P2-8 to servo bus)
                                      │  1 Mbps UART bus
                                      ▼
                        SO-ARM101 follower arm (6 × STS3215)
```

## Directory Structure

```text
firmware/nanocam_soarm/   ESP32-NanoCam follower firmware (PlatformIO)
  ├─ boards/nano_cam.json in-house board definition (16MB Flash / 8MB Octal PSRAM)
  ├─ src/                 firmware source (micro-ROS teleoperation + camera + audio + RGB)
  ├─ lib/microros/        micro-ROS static library (xtensa-lx7)
  └─ lib/scservo/         SCServo servo library (localized, no network dependency)
tools/                    PC-side scripts (wireless_teleoperate.py teleoperation bridge, follower_camera.py FPV receiver)
start_soarm_demo.sh       one-click startup script (network/Agent/calibration pre-check + teleoperation)
cali/                     leader/follower calibration files
docs/                     project progress and experiment records + hardware reference (docs/reference/)
```

## Features

- **micro-ROS wireless teleoperation**: `/joint_states` feedback at 20 Hz, `/joint_command` commands, full safety mechanisms (calibration check, pose handshake, soft limits, step-size limiting, watchdog)
- **Camera FPV**: MJPEG stream at `http://<IP>/stream` (GC2145 QVGA), published on the PC side as `sensor_msgs/CompressedImage`
- **Microphone**: volume level → `/follower_audio/level` (Float32, 5 Hz)
- **Speaker**: startup/ready/unlock/error tones (ES8311)
- **RGB status LED**: red on startup → orange on WiFi → green on micro-ROS → blue on unlock (WS2812)

## Quick Start

```bash
# 1. Configure the firmware WiFi (firmware/nanocam_soarm/src/wifi_config.h) and flash it
cd firmware/nanocam_soarm
# Open src/wifi_config.example.h, copy it to wifi_config.h, and fill in the SSID/password/Ubuntu PC IP
python3 -m platformio run --target upload

# 2. Ubuntu PC: start the micro-ROS Agent
snap run micro-ros-agent udp4 --port 8888

# 3. Teleoperation (in another terminal, leader arm connected to the USB servo driver board)
./start_soarm_demo.sh --check  # pre-flight check
./start_soarm_demo.sh

# 4. (Optional) Camera FPV
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
```

For full instructions, see [README_NANOCAM.md](./README_NANOCAM.md).

## Hardware Highlights

### Wiring

Between the ESP32-NanoCam and the follower arm, the link is **relayed through the servo driver board's UART pins**:

```text
Servo driver board UART:  RX ←── to NanoCam TX (P2-8 / GPIO20)
                          TX ──→ to NanoCam RX (P2-7 / GPIO19)
                         GND ──→ to NanoCam GND
```

- **TX to RX and RX to TX** (crossover), common GND, 1 Mbps baud rate;
- The ESP32-NanoCam's UART1 uses the module's **P2-7 (GPIO19, RX) / P2-8 (GPIO20, TX)**.

### Power

| Device | Power supply |
|---|---|
| ESP32-NanoCam | **Powered over the USB data cable** (the CH340K debug serial port works at the same time) |
| Follower arm (6×STS3215) | **12V 5A** external power supply |
| Leader arm (connected to the Ubuntu PC) | **5V 6A** external power supply |

> ⚠️ USB cannot power 6 servos, so the follower arm must use a 12V 5A external supply; the ESP32 is powered over the USB data cable.

### Other

- Camera GC2145 DVP; audio ES8311 (I2S 24kHz); RGB WS2812 @ GPIO18
- The firmware calibration is aligned with `cali/follower_recal.json`; after replacing the follower arm you must recalibrate and update the three arrays in `servo_bus.cpp` accordingly

## Origin

This project's follower controller evolved from the ESP32-S3 to our in-house ESP32-NanoCam.
