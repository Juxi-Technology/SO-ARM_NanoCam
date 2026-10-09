[English](complete-guide.md) | 简体中文

# SO-ARM101 无线遥操作项目 —— 完整运行教程

> 面向 ESP32-NanoCam 版（ESP32-S3 N16R8）从臂控制器。覆盖环境要求、安装、烧录、标定、运行全流程与排障。适用于 Linux（推荐 Ubuntu 22.04）和 Windows 两端协作：
> **编译烧录在任意一台机器都行，最终遥操作运行在 Ubuntu 上**。

---

## 一、系统架构与运行原理

```
SO-ARM101 主臂(leader) → USB 舵机驱动板 → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                            │  2.4 GHz Wi-Fi（同一局域网）
                                            ▼
                              ESP32-NanoCam 从臂控制器（ESP32-S3）
                                            │  1 Mbps UART（经舵机驱动板 UART 针脚中转）
                                            ▼
                              SO-ARM101 从臂(follower) 6×STS3215
```

- 主臂操作者动作 → LeRobot 读主臂 → ROS2 话题 `/joint_command` → micro-ROS Agent
  通过 UDP 8888 发送 → ESP32-NanoCam 收下并驱动 6 个舵机；
- 从臂反馈 `/joint_states`（20Hz）反向回传，作为闭环与看门狗；
- 板载摄像头发布 MJPEG 流 `http://<IP>/stream`（FPV），PC 端可转成 ROS 话题。

**角色分工**：主臂接 Ubuntu 电脑；从臂由 ESP32-NanoCam 控制，两者无线连接。

---

## 二、硬件清单

| 硬件 | 数量 | 说明 |
|---|---|---|
| SO-ARM101 主臂 | 1 | 带 6×STS3215 舵机 |
| SO-ARM101 从臂 | 1 | 带 6×STS3215 舵机 |
| ESP32-NanoCam 模块 | 1 | ESP32-S3 N16R8，板载摄像头/音频/RGB |
| USB 舵机驱动板 | 2 | 标定 + 主从臂总线中转（UART 针脚） |
| Ubuntu 22.04 电脑 | 1 | 运行 LeRobot + ROS2 + Agent |
| 2.4GHz 路由器或手机热点 | 1 | 主臂电脑与 NanoCam 同一局域网 |
| 12V 5A 外部电源 | 1 | **从臂供电**（USB 带不动 6 个舵机） |
| 5V 6A 外部电源 | 1 | **主臂供电**（连接 Ubuntu 电脑） |
| USB-C 数据线 | 2 | NanoCam 供电/调试 + 主臂驱动板连电脑 |

### 接线方式（重要）

ESP32-NanoCam 与从臂之间**通过舵机驱动板的 UART 针脚中转**：

```text
舵机驱动板 UART:   RX ←── NanoCam TX (P2-8 / GPIO20)
                   TX ──→ NanoCam RX (P2-7 / GPIO19)
                  GND ──→ NanoCam GND
```

- **TX 连 RX、RX 连 TX（交叉）**，GND 共地，1 Mbps 波特率；
- NanoCam 舵机总线走 UART1（模块 **P2-7 / P2-8**）；
- 调试串口走 USB-C（CH340K → UART0，Linux 下 `/dev/ttyUSB0`）。

### 供电

| 设备 | 供电 |
|---|---|
| ESP32-NanoCam | **USB 数据线供电**（CH340K 调试串口同时工作） |
| 从臂（6×STS3215） | **12V 5A** 外部电源 |
| 主臂（连 Ubuntu） | **5V 6A** 外部电源 |

---

## 三、环境要求

### 3.1 编译烧录环境（Windows 或 Linux 均可）

| 项 | 要求 |
|---|---|
| 操作系统 | Windows 10/11 或 Linux（macOS 也可） |
| Python | 3.8+（`python --version` 验证） |
| PlatformIO | Core 6.x（含 esp32s3 工具链 + Arduino 框架） |
| 磁盘空间 | 至少 3 GB 空闲 |
| 网络 | 能访问 GitHub / Espressif CDN（首次下载工具链约 1-2 GB） |

### 3.2 运行环境（Ubuntu 22.04 电脑，最终跑遥操作的地方）

| 项 | 要求 |
|---|---|
| 操作系统 | Ubuntu 22.04（64 位） |
| ROS 2 | Humble（Hawksbill） |
| LeRobot | 含 Feetech SO-101 支持（`so101_leader` / `so101_follower`） |
| micro-ROS Agent | `snap run micro-ros-agent` 或源码安装 |
| 依赖命令 | `nmcli`、`ip`、`flock`（NetworkManager、iproute2、util-linux 自带） |
| Python 环境 | `lerobot_so101` 虚拟环境（conda/miniforge） |

> 调试串口识别：Linux 下 NanoCam 是 `/dev/ttyUSB0`（CH340 驱动内核自带）。

---

## 四、环境安装

### 4.1 安装 PlatformIO（编译烧录端）

**方式 A：VSCode 扩展（推荐）**
1. 安装 [VSCode](https://code.visualstudio.com/)；
2. 扩展商店搜索 **PlatformIO IDE** 并安装，装完自动重启并下载 PlatformIO Core；
3. 验证：`pio --version`（VSCode 终端里）。

**方式 B：命令行安装**
```bash
pip install platformio
```

> Windows 下 `pio` 命令若在 Git Bash 里找不到，改用 PowerShell/CMD 终端，或把
> `C:\Users\<用户名>\.platformio\penv\Scripts` 加入 PATH。

### 4.2 首次构建自动下载工具链

进入固件目录跑一次编译（不烧录）：
```bash
cd firmware/nanocam_soarm
pio run
```
首次会依次下载：
1. espressif32 平台（`espressif32@7.0.1`）；
2. **工具链** `toolchain-xtensa-esp32s3`（约 100 MB，从 Espressif CDN 下载）；
3. Arduino 框架 `framework-arduinoespressif32`（约 200 MB）。

**下载慢/卡住的处理**：

- 耐心观察：PlatformIO 剩余时间估算不准，常卡住一段时间后突然跳完；给 5 分钟观察百分比是否推进；
- 开代理/VPN（走系统代理）；
- 手动下载工具链：用浏览器下载
  `https://dl.espressif.com/dl/xtensa-esp32s3-elf-gcc8_4_0-esp-2021r2-patch5-win32.zip`
  （Linux 对应 `-linux-amd64.tar.gz`），解压后把目录改名为`toolchain-xtensa-esp32s3` 放入 `C:\Users\<用户名>\.platformio\packages\`，重跑 `pio run`。
- 中途 `Ctrl+C` 中断不会损坏环境，重跑会续传。

### 4.3 安装 Ubuntu 运行环境

```bash
# 1. ROS 2 Humble（按官方文档）
#    https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html
# 安装完成后：
source /opt/ros/humble/setup.bash

# 2. LeRobot（含 Feetech 支持）
conda create -n lerobot_so101 python=3.10 -y
conda activate lerobot_so101
pip install lerobot[feetech]

# 3. micro-ROS Agent
sudo snap install micro-ros-agent
snap run micro-ros-agent udp4 --port 8888   # 测试能否启动

# 4. PlatformIO（Ubuntu 端若也要编译烧录）
pip install platformio
```

---

## 五、配置固件并烧录

micro-ROS 静态库位于 `lib/microros/`，本工程为 **xtensa-lx7 (ESP32-S3)** 版本。

### 5.1 配置 WiFi（二选一）

**方式一：编译期配置（默认）**
```bash
cd firmware/nanocam_soarm
cp src/wifi_config.example.h src/wifi_config.h
# 编辑 wifi_config.h：WIFI_SSID / WIFI_PASS / AGENT_IP（Ubuntu 电脑局域网 IP）
```

**方式二：串口命令配置（推荐，不用重烧）**
烧录后通过调试串口输入（115200 波特率）：
```text
wifi_ssid:你的热点名
wifi_pass:你的密码
agent_ip:Ubuntu电脑IP
wifi_show    # 查看当前生效配置
wifi_clear   # 清除，恢复编译期默认
```
任意设置命令保存后 **3 秒自动重启生效**。优先级：串口保存 > 编译期默认。

### 5.2 烧录

```bash
cd firmware/nanocam_soarm
pio run --target upload
```

**进下载模式（关键）**：

- NanoCam 走 CH340K → UART0 串口下载（非 USB-CDC 自动下载）；
- 先直接跑 upload，若板子有自动下载电路会直接成功；
- 若提示连接不上：**按住 BOOT 键（GPIO0）→ 插 USB（或按复位）→ 松开 BOOT**，立即重跑 upload；
- Windows 下若没自动识别串口，在 `platformio.ini` 的 `[env:nano_cam]` 加一行：
  `upload_port = COM3`（替换成设备管理器里 CH340 的实际 COM 号）。

**查看串口日志**：

```bash
pio device monitor --baud 115200
```

**烧录后应看到（按序出现）**：
```
audio: ES8311 ready @24000Hz      ← 音频初始化成功
Servo Ping mask: 0x3f             ← 6 个舵机全部在线
Servo calibration match: YES      ← 标定数组与舵机 EEPROM 一致
IP: 192.168.x.x  RSSI: -xx        ← WiFi 已连
Waiting for micro-ROS Agent...    ← 等待 Agent（下一步启动后消失）
```

> 舵机总线烧录时可空着，烧录与舵机运行互不干扰（UART0 调试 / UART1 舵机独立）。

---

## 六、标定两台机械臂（Ubuntu 上，仅更换硬件时）

仓库 `cali/` 已含主臂/从臂标定文件，固件标定数组也与从臂标定一致。**只有更换从臂/主臂硬件时才需要重新标定**。

```bash
# 从臂
python -m lerobot.scripts.lerobot_calibrate \
  --robot.type=so101_follower --robot.port=/dev/ttyACM0 \
  --robot.id=follower_recal --robot.calibration_dir="$PWD/cali"

# 主臂
python -m lerobot.scripts.lerobot_calibrate \
  --teleop.type=so101_leader --teleop.port=/dev/ttyACM0 \
  --teleop.id=leader_recal --teleop.calibration_dir="$PWD/cali"
```

重新标定从臂后，必须同步更新
`firmware/nanocam_soarm/src/servo_bus.cpp` 里的 `kHomingOffsets` / `kRangeMin` /`kRangeMax` 三个数组（顺序：shoulder_pan, shoulder_lift, elbow_flex, wrist_flex,wrist_roll, gripper），然后重新编译烧录。

---

## 七、运行流程（Ubuntu 上）

### 7.1 启动前检查

```bash
# 1. Ubuntu 电脑连上与 NanoCam 相同的 2.4GHz WiFi
# 2. 主臂 USB 舵机驱动板已连接并识别
ls -l /dev/ttyACM*   # 找到主臂串口
# 3. 从臂 NanoCam 已上电并联网（串口或浏览器确认 MJPEG 流可访问）
```

### 7.2 一键启动遥操作

```bash
# 设置环境（或直接编辑 start_soarm_demo.sh 顶部的默认值）
export SOARM_WIFI_SSID="你的2.4G热点"
export SOARM_AGENT_IP="Ubuntu电脑IP"
export SOARM_LEADER_PORT="/dev/ttyACM*"
export SOARM_PYTHON="$(command -v python)"   # lerobot_so101 环境

./start_soarm_demo.sh --check    # 预飞检查：网络/主臂/Agent/从臂在线
./start_soarm_demo.sh            # 正式启动遥操作，Ctrl+C 停止
```

脚本会按顺序做：
1. 检查网络（SSID 必须匹配 `EXPECTED_WIFI_SSID`）、主臂串口、标定文件存在；
2. 启动 micro-ROS Agent（若未运行，日志在 `logs/micro_ros_agent.log`）；
3. 等从臂 `/joint_states` 上线（15s 超时）；
4. 主臂动作 → 从臂跟随，30Hz 命令频率，**`--mapping-mode absolute`（绝对映射）**。

**关于 absolute 映射**：主臂姿态与从臂姿态在各自标定坐标系里一一对应。好处是
**断连重连后无累积偏差**——重连时从臂在 8 秒内平滑对齐主臂当前姿态（`startup_blend`），之后主臂回零 → 从臂也回自己零位。曾用 `relative`（相对）映射，但断连重连后从臂停留在断连位置，与回到零位的主臂产生永久偏差，故改为 absolute。

**关于 Agent 失联自动重启**（固件 2026-08-19 后）：Ctrl+C 停掉遥操作后，从臂约10 秒内自动重启回到 `Waiting for micro-ROS Agent...`。因此 Ctrl+C 后可直接重跑本脚本，无需手动复位从臂。重连期间从臂会回零位（重新上电）。

### 7.3 摄像头 FPV（可选）

> GC2145 无硬件 JPEG 编码器，固件用 **RGB565 采集 + `frame2jpg` 软件编码**。正式配置为 **QVGA 320×240**（VGA 在此板 DVP 上数据率过高会花屏，见第九节排障）。帧率比硬件 JPEG 低，属正常；`/stream` 与 `/jpg` 不能同时访问（httpd 单任务）。

```bash
# 终端 2（Ubuntu）：
source /opt/ros/humble/setup.bash
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
# 验证
ros2 topic hz /follower_camera/image_raw/compressed    # 实测约 10~15 Hz
# 不装 ROS 也能验证链路：浏览器打开 http://<NANOCAM_IP>/stream
```

### 7.4 手动验证各话题

```bash
ros2 topic echo /joint_states --once           # 从臂反馈
ros2 topic hz /joint_states                    # 应约 20 Hz
ros2 topic echo /follower_audio/level --once   # 麦克风电平（说话时抬升）
```

---

## 八、运行状态灯与提示音

| RGB 颜色 | 状态 |
|---|---|
| 红 | 启动中 / micro-ROS 初始化失败 / WiFi 丢失 |
| 橙 | WiFi 已连，等待 micro-ROS Agent |
| 绿 | micro-ROS 就绪（遥操作链路通） |
| 蓝 | 舵机控制已解锁（ARMED） |
| 紫 | 控制命令被拒绝（握手/限位/步长不匹配） |

扬声器提示音：上电"嘀嘀"两声；micro-ROS 就绪/舵机解锁：上扬双音；初始化失败：一声低音。

**安全机制（固件内置，无需手动）**：舵机身份检查、EEPROM 标定检查、当前姿态握手（0.05 rad）、软限位、单命令步长 0.25 rad、反馈看门狗 0.5 s、WiFi 丢失 10 s 超时重启。

---

## 九、注意事项

### 电源与接线
- **从臂必须外部 12V 5A 供电**，USB 带不动 6 个 STS3215；舵机总线与舵机电源共地；
- **主臂 5V 6A 供电**（连接 Ubuntu 电脑）；**ESP32-NanoCam 用 USB 数据线供电**；
- 从臂舵机总线经舵机驱动板 UART 接 **P2-7（RX）/ P2-8（TX）**，不要接到 UART0 的 43/44（那是调试串口）；
- 飞行演示时：倒挂安装后需重新确认关节方向、重心、供电（BEC）方案，并做 EMI 干扰测试。

### 通信
- NanoCam 与 Ubuntu 电脑必须在**同一 2.4GHz 局域网**（手机热点即可）；客户端隔离需关闭；
- NanoCam 天线在模块上（U.FL），RSSI 差时先看天线与摆放；建议做 5/10/20/30 米距离测试；
- `wifi_config.h` 含 WiFi 密码，已被 .gitignore 排除，**不要提交**。

### 编译/代码
- 改动代码必须同步更新 README（项目维护约定，见仓库根 README「维护约定」对照表）；
- 重建 micro-ROS 库仅在你改消息类型/QoS/内存池时需要（见 README_NANOCAM.md 第五节），
  日常开发**不需要**重编 `libmicroros.a`；
- 上板验证通过的重要版本，提交信息注明"上板验证通过"，作为可回退点。

### 排障速查
| 现象 | 排查 |
|---|---|
| 烧录连不上 | 手动进下载模式（BOOT+复位）；`platformio.ini` 加 `upload_port` |
| 烧录后无串口输出 | 检查 USB 线与 CH340 驱动；Windows 看设备管理器 COM 口 |
| 卡在 `Waiting for micro-ROS Agent...` | 查 AGENT_IP / UDP 8888 / 客户端隔离 |
| 舵机总线无响应（servo_mask≠0x3f） | 确认经舵机驱动板 UART 接 P2-7/P2-8；从臂 12V 5A 外部供电 |
| 麦克风电平恒 0 | 看 `audio: ES8311 ready` 日志；I2C 41/42 上拉；对麦克风吹气验证 |
| 扬声器无声 | 检查喇叭连接；ES8311 音量寄存器 `R_DAC32`（当前 0x30） |
| WiFi 经常断 | 检查天线、距离；RGB 变红表示 WiFi 丢失，10s 后自动重启 |

### 摄像头专项排障（上板实测经验）
| 现象 | 根因 | 修复 |
|---|---|---|
| `i2c driver install error` + `camera probe failed` | **I2C 冲突**：ES8311 用 `Wire1` 占用 GPIO41/42，摄像头 SCCB 再装 I2C 驱动被拒 | `audio_es8311.cpp` 的 `init()` 末尾加 `Wire1.end()`，释放 I2C 给摄像头（已在固件中修复） |
| `JPEG format is not supported on this sensor` (0x106) | **GC2145 无硬件 JPEG 编码器**（仅 OV2640/OV5640 有） | 采集改用 `PIXFORMAT_RGB565`，`/stream` 和 `/jpg` 用 `frame2jpg` 软件编码成 JPEG（已在固件中修复） |
| `/jpg`、`/stream` 无响应，浏览器一直转圈 | **httpd 栈溢出**：默认栈 8KB 容不下 `frame2jpg` 软编码 | `start_server()` 里 `config.stack_size = 16384`（已在固件中修复） |
| `/stream` 打开但黑屏 | **缺 multipart 边界**：每帧之间没发 `STREAM_BOUNDARY`，浏览器无法解析 | 每帧发送前补发 `STREAM_BOUNDARY`（已在固件中修复） |
| curl 测 `/jpg` 返回 `HTTP:000`，但浏览器能出图 | esp_http_server **单任务**：`/stream` 占用 httpd 任务时 `/jpg` 排不上队；或 curl 超时短 | 关掉 `/stream` 再单独测 `/jpg`；用浏览器代替 curl 验证 |
| 摄像头初始化成功但全黑/无帧 | 多为**硬件**：AVDD/DOVDD 供电、PWDN 电平、排线接触 | 先用浏览器 `/jpg` 测快照（能出图=链路通）；查摄像头 2.8V 供电与排线 |
| VGA 画面下方约 2/3 花屏 | **DVP 数据率过高**：VGA RGB565 超出此板 DVP 采样时序余量（24/20/16MHz × 单/双缓冲均复现）；QVGA 正常 | 正式配置用 **QVGA 320×240**（FPV 够用），或换更稳的 XCLK/改 DVP 硬件走线 |

### 音频专项排障（上板实测经验）
| 现象 | 根因 | 修复 |
|---|---|---|
| 扬声器**完全无声** + 麦克风电平 ≈ 0（如 `0.0009`） | **MCLK 没输出**：legacy I2S 驱动在 ESP32-S3 上不产生 MCLK，ES8311 内部 DAC/ADC 无时钟 | 用 **LEDC 在 GPIO39 生成 6.15MHz MCLK**（`audio_es8311.cpp` 的 `start_ledc_mclk()`，已在固件中修复） |
| 提示音**太小**（贴耳才听到） | 数字振幅低 + ES8311 主音量小 | `play_tone` 振幅 12000→30000、`R_DAC32` 0x30→0xFF（已在固件中修复，约 +29dB） |
| 上电只有启动"嘀嘀"，无其他提示音 | **正常现象**：就绪/解锁提示音是事件驱动，需跑遥操作才触发 | 启动音=上电即播；就绪音=Agent 通信建立；解锁音=收到控制命令 |

**注意**：esp_http_server 是单任务的，`/stream` 和 `/jpg` **不能同时访问**——开着 `/stream` 时 `/jpg` 会一直挂起。抓单帧前先关掉流页面。

---

## 十、当前开发状态提醒

- **已上板验证通过的部分**（2026-08-19）：
  - ✅ 音频 `ES8311 ready @24000Hz`（MCLK 输出正常 + 扬声器/麦克风均正常，修复了 MCLK 缺失 + 音量过小，见第九节）
  - ✅ WiFi 连接 + micro-ROS 通信（`/joint_states` 20Hz 稳定、`/follower_audio/level` 正常）
  - ✅ GC2145 摄像头 FPV：QVGA `/stream` 完整流畅（修复了 I2C 冲突 / 软编码 / httpd 栈 / multipart 边界，见第九节）
  - ✅ 完整遥操作链路（主臂跟随从臂）
  - ✅ **absolute 映射 + Agent 失联自动重启**：断连重连后主从臂对齐无偏差；Ctrl+C 后从臂自动重启等待重连（见第七节）
- **仍待验证**：
  - ⚠️ 飞行场景：倒挂安装方向、重心、供电（BEC）、EMI 干扰
- 详细文档：仓库根目录 `README.md`（架构/维护约定）、`README_NANOCAM.md`（硬件/外设/重建库），
  进度记录见 `docs/soarm_project_progress_*.md`。
