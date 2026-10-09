[English](README_NANOCAM.md) | 简体中文

# 工程说明

该项目适配**自研 ESP32-NanoCam 模块**（ESP32-S3 N16R8，板载 DVP 摄像头 / ES8311 音频 / WS2812 RGB）。

## 一、系统架构

```
SO-ARM101 主臂 → USB 舵机驱动板 → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                      │  2.4 GHz Wi-Fi（同一局域网）
                                      ▼
                        ESP32-NanoCam 从臂控制器（ESP32-S3）
                                      │  1 Mbps UART 舵机总线
                                      ▼
                        SO-ARM101 从臂 (6 × STS3215)
```

PC 与 NanoCam 必须在同一局域网（2.4 GHz Wi-Fi，手机热点即可）。

## 二、硬件适配说明（NanoCam）

### 2.1 引脚映射

> 引脚定义来自 [docs/reference/nano_config.h](./docs/reference/nano_config.h) 与硬件原理图文档。

| 外设 | 引脚 |
|---|---|
| 舵机总线 (UART1) | **TX=GPIO20 (P2-8 ESP_P)，RX=GPIO19 (P2-7 ESP_N)** ← 模块 P2 排针 |
| 调试串口 (UART0) | GPIO43/44 → 板载 CH340K → USB-C（无原生 USB CDC） |
| 摄像头 DVP (GC2145) | D0~D7=GPIO4/2/1/3/5/7/8/10，PCLK=6，VSYNC=13，HREF=11，XCLK=9 (24MHz)，PWDN=12，RESET=14，SCCB SDA/SCL=41/42 |
| 音频 ES8311 (I2S) | MCLK=39，BCLK=38，WS=47，DIN(ADC)=40，DOUT(DAC)=48；I2C SDA/SCL=41/42，地址 0x30 |
| 麦克风 | AP2718AT 模拟 MEMS（经 ES8311 ADC） |
| 扬声器 | NS4150B D 类功放（经 ES8311 DAC），板上无 PA 使能脚 |
| RGB | WS2812 @ GPIO18（1 颗，GRB，RMT 驱动） |
| BOOT | GPIO0 |

### 2.2 与早期版的差异

| 项目 | 本工程 (ESP32-NanoCam) |
|---|---|
| 板卡定义 | 自建 `boards/nano_cam.json`（16MB Flash / 8MB Octal PSRAM，qio_opi） |
| 舵机总线 | Serial1/UART1，TX=20/RX=19（UART0 被 CH340K 调试占用） |
| 调试串口 | UART0 (43/44) → CH340K → USB-C |
| 摄像头 | NanoCam DVP GC2145（GPIO1~14 + 41/42），XCLK 24MHz |
| 音频 | ES8311 + AP2718AT 麦克风 + NS4150B 扬声器（新增） |
| RGB | WS2812 状态灯（新增） |
| micro-ROS 库 | xtensa-lx7——NanoCam 同为 ESP32-S3，与 S3 版通用 |
| PC 端脚本 | 不变（tools/、start_soarm_demo.sh 与硬件无关） |

### 2.3 舵机总线接线（重要）

NanoCam 的 UART0 被板载 CH340K 占用（USB 调试串口），所以舵机总线走
**UART1 映射到模块 P2 排针的扩展串口**。从臂通过**舵机驱动板的 UART 针脚中转**连接：

```text
舵机驱动板 UART:   RX ←── NanoCam TX (P2-8 / GPIO20)
                   TX ──→ NanoCam RX (P2-7 / GPIO19)
                  GND ──→ NanoCam GND
```

- **TX 连 RX、RX 连 TX（交叉）**，GND 共地，1 Mbps 波特率；
- 舵机总线与舵机电源共地（从臂 12V 5A 电源）。

### 2.4 供电说明

| 设备 | 供电方式 |
|---|---|
| ESP32-NanoCam | **USB 数据线供电**（CH340K 调试串口同时工作） |
| 从臂（6×STS3215） | **12V 5A** 外部电源 |
| 主臂（连接 Ubuntu 电脑） | **5V 6A** 外部电源 |

> ⚠️ USB 带不动 6 个舵机，从臂必须 12V 5A 外部供电；ESP32 用 USB 数据线供电即可。

### 2.5 头文件路径与 build_flags

micro-ROS 头文件树是扁平结构（`include/<pkg>/<header>.h`），只保留 `-Ilib/microros/include` 根路径。**不要**添加逐包 `-Ilib/microros/include/<pkg>/` 路径——那会把 `<string.h>` 解析成 `rosidl_runtime_c/string.h`、把 WiFi 库的 `<Client.h>` 解析成 `rcl/Client.h`，导致编译失败。

## 三、板载外设一览

固件上电后的完整功能：

| 功能 | 实现 | 说明 |
|---|---|---|
| micro-ROS 遥操作 | `main.cpp` + `servo_bus.cpp` | `/joint_states` 20Hz、`/joint_command` 接收，安全机制不变 |
| 摄像头 FPV | `camera_stream.cpp` | `http://<IP>/stream` MJPEG 流（QVGA） |
| 麦克风 | `audio_es8311.cpp` | 环境音量电平 → `/follower_audio/level`（Float32，5Hz） |
| 扬声器 | `audio_es8311.cpp` | 启动/就绪/解锁/错误提示音 |
| RGB 状态灯 | `rgb_status.cpp` | 启动红 → WiFi 橙 → micro-ROS 绿 → 解锁蓝；WiFi 丢失红 |

### RGB 状态含义

| 颜色 | 状态 |
|---|---|
| 红 | 启动中 / micro-ROS 初始化失败 / WiFi 丢失 |
| 橙 | WiFi 已连接，等待 micro-ROS Agent |
| 绿 | micro-ROS 就绪（遥操作链路通） |
| 蓝 | 舵机控制已解锁（ARMED） |
| 紫 | 控制命令被拒绝（握手/限位/步长不匹配） |

### 扬声器提示音

- 上电：两声短"嘀嘀"（启动音）
- micro-ROS 就绪：上扬双音
- 舵机解锁：上扬双音
- 初始化失败：一声低音

## 四、快速开始

### 1. 标定两台机械臂（Ubuntu 22.04 上）

先装好 ROS 2 Humble、LeRobot（含 Feetech 支持）、micro-ROS Agent、PlatformIO。
然后用 USB 总线舵机驱动板分别标定从臂和主臂：

```bash
python -m lerobot.scripts.lerobot_calibrate \
  --robot.type=so101_follower \
  --robot.port=/dev/ttyACM0 \
  --robot.id=follower_recal \
  --robot.calibration_dir="$PWD/cali"

python -m lerobot.scripts.lerobot_calibrate \
  --teleop.type=so101_leader \
  --teleop.port=/dev/ttyACM0 \
  --teleop.id=leader_recal \
  --teleop.calibration_dir="$PWD/cali"
```

### 2. 把从臂标定写入固件

打开 `firmware/nanocam_soarm/src/servo_bus.cpp`，把 `kHomingOffsets` / `kRangeMin` / `kRangeMax` 三个数组替换成你自己 `cali/follower_recal.json` 的数值（顺序：shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll, gripper）。

> 后续如果更换从臂时需要重新标定并更新固件数组，重新编译烧录。

### 3. 配置 WiFi 并烧录

**方式一：编译期配置（默认）**

```bash
cd firmware/nanocam_soarm
cp src/wifi_config.example.h src/wifi_config.h
# 编辑 wifi_config.h: WIFI_SSID / WIFI_PASS / AGENT_IP（Ubuntu PC 的局域网 IP）

python3 -m platformio run
python3 -m platformio run --target upload
python3 -m platformio device monitor --baud 115200
```

**方式二：串口命令配置（推荐，无需重烧）**

固件内置运行时配置（NVS 存储），通过调试串口（115200，`pio device monitor`）随时输入：

```text
wifi_ssid:MyWiFi       设置 WiFi 名称并保存
wifi_pass:MyPassword   设置 WiFi 密码并保存
agent_ip:192.168.1.100 设置 micro-ROS Agent IP 并保存
wifi_show              查看当前生效配置
wifi_clear             清除已保存配置，恢复编译期默认
```

任意设置命令保存后 3 秒自动重启生效。优先级：**串口保存的配置 > 编译期默认**。换热点/换电脑只需插 USB 敲三条命令，不用改代码重烧。

> 注意：`wifi_config.h` 的编译期默认值仍然保留（未用串口配置过时的回退）；`wifi_show` 会区分"来自 NVS"和"编译期默认"。密码以明文存于 NVS，局域网演示场景可接受。

> NanoCam 的 USB 接口是 CH340K 转 UART0，Linux 下设备名通常是 `/dev/ttyUSB0`（或 `/dev/serial/by-id/...CH340*`），PlatformIO 可自动识别（board json 已配 HWID 0x1A86:0x7523）。串口监视器波特率 115200。

成功启动日志应包含：

```
Servo Ping mask: 0x3f (expected 0x3f)
Servo calibration match: YES
audio: ES8311 ready @24000Hz
IP: 192.168.x.x  RSSI: -xx
Waiting for micro-ROS Agent...
```

### 4. 启动 micro-ROS Agent 并验证

```bash
source /opt/ros/humble/setup.bash
snap run micro-ros-agent udp4 --port 8888
```

NanoCam 串口出现 `micro-ROS ready` 后（RGB 变绿、扬声器响提示音）：

```bash
ros2 topic echo /joint_states --once
ros2 topic hz /joint_states          # 应约 20 Hz
ros2 topic echo /follower_audio/level --once   # 麦克风电平
```

### 5. 无线主从遥操作

```bash
export SOARM_WIFI_SSID="YOUR_2G4_WIFI_SSID"
export SOARM_AGENT_IP="YOUR_UBUNTU_PC_IP"
export SOARM_LEADER_PORT="/dev/ttyUSB*"
export SOARM_PYTHON="$(command -v python)"

./start_soarm_demo.sh --check   # 预飞检查
./start_soarm_demo.sh           # 启动遥操作，Ctrl+C 停止
```

安全机制：舵机身份检查、EEPROM 标定检查、当前姿态握手（0.05 rad）、软限位、单命令步长 0.25 rad、反馈看门狗 0.5 s、Wi-Fi 10 s 恢复超时重启。

## 五、重建 libmicroros.a（ESP32-S3 / xtensa-lx7）

> 本工程 `firmware/nanocam_soarm/lib/microros/` 已附上 ESP32-S3 版静态库（NanoCam 为 ESP32-S3，库通用）。**普通使用跳过本节**。只有当你需要自定义 micro-ROS 配置（消息类型、QoS、内存池等）时才需要重建。

### 方式 A：官方 Docker 构建器（推荐，可在任意机器上执行）

micro-ROS 官方 `micro_ros_arduino` 库的生成脚本自带 **esp32s3 目标**：

```bash
git clone -b humble https://github.com/micro-ROS/micro_ros_arduino.git
cd micro_ros_arduino
docker pull microros/micro_ros_static_library_builder:humble
docker run -it --rm -v $(pwd):/project \
  --env MICROROS_LIBRARY_FOLDER=extras \
  microros/micro_ros_static_library_builder:humble -p esp32s3
```

产物在 `src/esp32s3/libmicroros.a`，头文件在 `src/` 下各包目录中：

```bash
cp src/esp32s3/libmicroros.a <工程>/firmware/nanocam_soarm/lib/microros/
# 头文件整体替换（保留该目录下的 default_transport.cpp / wifi_transport.cpp /
# micro_ros_arduino.h 三个自定义文件）
rsync -a src/* <工程>/firmware/nanocam_soarm/lib/microros/include/ \
  --exclude esp32s3 --exclude '*.cpp' --exclude micro_ros_arduino.h
```

> **关于工具链**：官方脚本的 esp32s3 段默认使用 `xtensa-esp32-elf`（LX6）工具链编译，LX6/LX7 对普通 C 代码指令集兼容、可运行。本工程随附的 `libmicroros.a` 是用**纯正 LX7 工具链**（`xtensa-esp32s3-elf` gcc 8.4.0，与 PlatformIO 内置版本一致）编译的，做法：下载 `xtensa-esp32s3-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz`（Espressif crosstool-NG releases），解压后把 `library_generation.sh` 里 esp32s3 段的 `TOOLCHAIN_PREFIX` 改为 `/uros_ws/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-`，再挂载进容器重跑：

> ```bash
> docker run --platform linux/amd64 -it --rm \
>   -v $(pwd):/project \
>   -v <解压目录>/xtensa-esp32s3-elf:/uros_ws/xtensa-esp32s3-elf \
>   --env MICROROS_LIBRARY_FOLDER=extras \
>   microros/micro_ros_static_library_builder:humble -p esp32s3
> ```
>
> 注意：Apple Silicon 上必须加 `--platform linux/amd64`（镜像内置的 esp32 工具链是 x86_64 二进制，arm64 容器内无法执行）。

### 方式 B：Ubuntu 22.04 + ROS 2 Humble + PlatformIO 工具链

1. 确保 PlatformIO 已下载过 S3 工具链（在固件目录跑一次 `pio run` 即可）：

   ```bash
   ls ~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
   ls ~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3
   ```

2. 用 micro_ros_setup 拉取 micro-ROS 源码（与 `build_microros.sh` 的 `/tmp/firmware/mcu_ws` 布局一致）：

   ```bash
   mkdir -p /tmp/firmware && cd /tmp/firmware
   git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup
   # 安装 micro_ros_setup 依赖后:
   source /opt/ros/humble/setup.bash
   colcon build && source install/local_setup.bash
   ros2 run micro_ros_setup create_firmware_ws.sh generate_lib
   ```

3. 运行本工程的 S3 构建脚本：

   ```bash
   cd <工程>/firmware/nanocam_soarm
   chmod +x build_microros_s3.sh
   ./build_microros_s3.sh
   ```

   脚本已把 riscv32 → xtensa-esp32s3、`-march=rv32imc` → `-mlongcalls`、`esp32c3` SDK → `esp32s3` SDK。产物按脚本末尾提示拷入工程即可。

## 六、摄像头视频回传（FPV）

固件上电联网后自动启动 MJPEG 流媒体服务（板载 GC2145，DVP 接口）：

```text
http://<NANOCAM_IP>/         信息页
http://<NANOCAM_IP>/jpg      单帧 JPEG（快照）
http://<NANOCAM_IP>/stream   连续 MJPEG 流（FPV）
```

> 默认 HTTP 端口 **80**（固件 `HTTPD_DEFAULT_CONFIG()`）。

### 参数

- 分辨率 **QVGA 320×240**（正式配置），**RGB565 采集 + `frame2jpg` 软件编码**（GC2145 无硬件 JPEG，仅 OV2640/OV5640 有），质量 12，双缓冲放 **8MB Octal PSRAM**
- **为何用 QVGA**：实测 VGA(640×480) RGB565 在此板 DVP 上数据率过高，画面下方约 2/3 花屏（XCLK 24/20/16MHz × 单/双缓冲组合均复现）；QVGA 完整流畅
- 流媒体运行在独立 httpd 任务中（栈已调至 16KB 容纳软编码），与 micro-ROS 遥操作、音频采集互不干扰
- 想改分辨率/质量：编辑 `firmware/nanocam_soarm/src/camera_stream.cpp` 里的 `config.frame_size` / `kJpegQuality`；画面方向用 `set_vflip` / `set_hmirror` 调整（同文件）
- 摄像头初始化失败时固件打印一行提示后继续正常工作（遥操作不受影响）
- **注意**：esp_http_server 单任务，`/stream` 与 `/jpg` 不能同时访问（开着流时 `/jpg` 会挂起）

### PC 端接收（发布为 ROS 2 话题）

```bash
# 终端 1: 照常启动遥操作
./start_soarm_demo.sh

# 终端 2: 接收视频并发布话题
source /opt/ros/humble/setup.bash
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
# 可选: --topic /自定义话题  --max-fps 10

# 验证
ros2 topic hz /follower_camera/image_raw/compressed   # 应约 10~15 Hz
rviz2    # Add → By topic → Camera，选 /follower_camera/image_raw/compressed
```

不装 ROS 也能先验证链路：浏览器打开 `http://<NANOCAM_IP>/stream`，或 `curl -s http://<NANOCAM_IP>/jpg -o snap.jpg`。

## 七、音频与 RGB 状态灯

### 麦克风（AP2718AT → ES8311 ADC）

固件每 200ms 读取一次麦克风音量电平（RMS，归一化 0~1）并发布到 `/follower_audio/level`（`std_msgs/Float32`，best-effort）。可自行实现用于语音活动检测、环境监听、或作为"有人说话再抓取"的简单触发信号。

```bash
ros2 topic echo /follower_audio/level
```

### 扬声器（ES8311 DAC → NS4150B）

固件内置四组提示音（启动/就绪/解锁/错误，见第三节）。如需自定义提示音，修改 `firmware/nanocam_soarm/src/audio_es8311.cpp` 中的 `play_tone()` 调用。音量在 ES8311 寄存器 0x32（`R_DAC32 = 0xFF`，当前固件已设最大）。

### 音频说明

- 采样率 24 kHz、16-bit、立体声槽位（与 NanoCam 原厂固件一致），MCLK = 256×FS = 6.144 MHz
- **MCLK 由 LEDC 生成**（GPIO39，80MHz÷13≈6.154MHz，误差 0.16% 在容差内）：legacy I2S 驱动在 ESP32-S3 上不输出 MCLK，会导致扬声器无声 + 麦克风电平恒 0，已在 `audio_es8311.cpp` 的 `start_ledc_mclk()` 中用 LEDC 修复
- ES8311 控制走 I2C1（GPIO41/42 物理总线与摄像头 SCCB 共用，摄像头只在启动时使用 SCCB，运行期无冲突）；`init()` 末尾 `Wire1.end()` 释放 I2C 给摄像头
- 麦克风增益默认值同 NanoCam 原厂（寄存器 0x16 = 0x24），如需提高灵敏度可调 `audio_es8311.cpp` 中 `R_ADC16` 的值

## 八、与原版不同的排障点

- **`Waiting for micro-ROS Agent...` 卡住**：先查 AGENT_IP / UDP 8888 / 客户端隔离。NanoCam 的 U.FL 天线在模块上，RSSI 差时先看天线和摆放。
- **烧录后无串口输出**：NanoCam 的 USB 是 CH340K → UART0，Linux 下设备名 `/dev/ttyUSB0`；如果插上没识别，检查 USB 线和 CH340 驱动（内核自带）。
- **舵机总线无响应（servo_mask≠0x3f）**：确认舵机总线通过舵机驱动板 UART 接在 **P2-7/P2-8**（GPIO19/20）而不是 UART0 的 43/44；从臂必须 12V 5A 外部供电（USB 带不动）。
- **舵机总线与调试串口混淆**：调试串口是 USB-C（CH340K→UART0），与舵机总线完全独立，可以同时使用。
- **麦克风电平一直为 0**：检查 `audio: ES8311 ready` 日志；确认 MCLK 已输出（GPIO39 应有 ~1.65V，LEDC 生成）；I2C 总线 41/42 上拉（板上已有 10K）；对着麦克风吹气看 `/follower_audio/level` 是否跳动。
- **扬声器无声**：确认 NS4150B 喇叭接在扬声器连接器上；确认 GPIO39 MCLK 有输出（LEDC，`start_ledc_mclk()`）；音量寄存器 `R_DAC32`（当前 0xFF）；ES8311 未初始化时日志会打印失败原因。
- **RGB 灯不亮**：WS2812 数据脚是 GPIO18，检查固件启动日志是否出现 `camera_stream` 之前的 RMT 初始化错误（一般不会）。

## 九、测试结果（预期）

- 六个从舵全部识别（`servo_mask=0x3f`）
- `/joint_states` 约 20 Hz 发布
- 主控桥 30 Hz 发布命令
- 摄像头流 `http://<IP>/stream` QVGA 流畅
- `/follower_audio/level` 5 Hz 发布，说话时电平明显抬升
- RGB 状态灯随启动→联网→就绪→解锁逐级变化
- 拔掉 USB 数据线后（ESP32 独立供电，从臂外部 12V 供电）仍可运行

## 十、维护约定（改代码必须同步改文档）

**规则：任何代码改动，只要改变了用户可见行为，就必须在同一提交里更新相关文档。**

| 你改了什么                                             | 必须同步更新的文档                                           |
| ------------------------------------------------------ | ------------------------------------------------------------ |
| `src/main.cpp`（话题名/频率/状态机/RGB 颜色）          | `firmware/nanocam_soarm/README.md` 的「ROS 接口」「板载外设」节 |
| `src/servo_bus.cpp`（舵机引脚/标定/总线参数）          | 固件 README 差异表 + `README_NANOCAM.md` 引脚表/接线节       |
| `src/camera_stream.cpp`（分辨率/方向/端口）            | `README_NANOCAM.md` 第六节（FPV）                            |
| `src/audio_es8311.cpp`（提示音/增益/音量）             | `README_NANOCAM.md` 第七节（音频）                           |
| `platformio.ini` / `boards/nano_cam.json`（板卡/依赖） | 固件 README 硬件适配表                                       |
| `tools/`、`start_soarm_demo.sh`（PC 端）               | `README_NANOCAM.md` 第四/六节对应命令                        |
| 新增/删除 ROS 话题或消息类型                           | 固件 README「ROS 接口」表（改消息类型还需重建 libmicroros.a） |
| 上板验证通过 / 踩坑记录 / 新功能落地                   | 新建 `docs/soarm_project_progress_<日期>.md` 追加进度记录    |

**其他约定：**

- 提交格式：`git add -A && git commit -m "一句话说明改动和原因" && git push`
- 上板验证通过的重要版本，提交信息里注明（如"上板验证通过"），作为可回退点
- `wifi_config.h` 与 `.pio/` 永不提交（已在 .gitignore）
- 未验证的硬件假设在代码注释里标明"待上板验证"

## 十一、参考资料

- [NanoCam 硬件参考文档（原理图/规格书/引脚定义/ES8311 驱动）](./docs/reference/)
- [micro-ROS](https://micro.ros.org/) / [micro_ros_arduino](https://github.com/micro-ROS/micro_ros_arduino)
- [LeRobot](https://github.com/huggingface/lerobot)
