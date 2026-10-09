# SO-ARM101 无线遥操作项目进度（ESP32-NanoCam 适配阶段）

日期：2026-08-15

> 前序阶段记录见 [soarm_project_progress_2026-07-22.md](./soarm_project_progress_2026-07-22.md)
> （早期固件阶段：标定、速度优化、一键启动、通信方案决策）。

## 1. 硬件演进

从臂控制器经过三次演进：

```text
原版 ESP32 总线舵机适配板（参考方案）
   ↓ 芯片升级
ESP32-S3 Sense（验证摄像头可行性）
   ↓ 自研模块
ESP32-NanoCam（ESP32-S3 N16R8，最终方案）
   ├─ 板载 DVP 摄像头（GC2145，兼容 OV2640/OV5640）
   ├─ ES8311 音频编解码 + AP2718AT 麦克风 + NS4150B 扬声器
   ├─ WS2812 RGB 状态灯（GPIO18）
   ├─ CH340K USB 调试串口（UART0，GPIO43/44）
   └─ 扩展串口排针（GPIO19/20，本项目用作舵机总线）
```

## 2. NanoCam 适配完成内容

### 2.1 固件（firmware/nanocam_soarm，PlatformIO + Arduino）

| 模块 | 说明 |
|---|---|
| micro-ROS 遥操作 | 继承原版逻辑，安全机制不变（标定检查/握手/限位/步长/看门狗） |
| 舵机总线 | UART0 → **UART1 映射 GPIO19/20**（UART0 被 CH340K 占用），1Mbps |
| 板卡定义 | 自建 `boards/nano_cam.json`（16MB Flash / 8MB Octal PSRAM） |
| micro-ROS 库 | xtensa-lx7 版 `libmicroros.a`（与 S3 版通用，无需重编） |
| 摄像头 FPV | GC2145 DVP（GPIO1~14+41/42），MJPEG 流 `http://<IP>:81/stream`，VGA@10-15fps |
| 麦克风 | ES8311 ADC → 音量电平发布 `/follower_audio/level`（Float32，5Hz） |
| 扬声器 | ES8311 DAC → NS4150B，四组提示音（启动/就绪/解锁/错误） |
| RGB 状态灯 | WS2812（RMT），启动红→WiFi 橙→micro-ROS 绿→解锁蓝 |
| 依赖本地化 | SCServo 库 vendor 进 `lib/scservo/`，构建零网络依赖 |

### 2.2 PC 端（tools/）

- `wireless_teleoperate.py`：遥操作桥（原版继承，未改动）。
- `follower_camera.py`：新增，FPV 接收节点，发布
  `sensor_msgs/CompressedImage`（topic: `/follower_camera/image_raw/compressed`）。

### 2.3 代码审查

固件适配完成后做了一次专门审查，修复 4 个问题：

1. 麦克风读取超时 50ms → 10ms（防止未锁定时阻塞 20Hz 关节发布）；
2. 明确 audio init 必须先于 camera init（GPIO41/42 被摄像头 SCCB 重新路由）；
3. `rcl_publish` 返回值检查（消除编译警告）；
4. RMT 初始化失败提示。

## 3. 验证状态

### 已通过

- 固件全新建构建通过：RAM 22.4%（73KB）、Flash 13.6%（894KB）。
- 符号级链接验证：audio/rgb/camera/舵机/micro-ROS 全部进入最终 ELF。
- 引脚无冲突核对（摄像头 1~14/41/42、音频 38~40/47/48、RGB 18、舵机 19/20、调试 43/44）。
- S3 摄像头走 LCD_CAM 外设，与音频 I2S0 无外设冲突（soc_caps.h 验证）。
- ES8311 初始化序列与 Espressif 官方驱动逐寄存器核对。
- GitHub 仓库 `SO-ARM101-NanoCam`（私有）已建立并推送。

### 待上板验证（烧录后按顺序检查串口日志）

```text
audio: ES8311 ready @24000Hz      ← 音频 I2C + I2S 初始化成功
Servo Ping mask: 0x3f             ← 六个舵机在线（舵机总线接 P2-7/P2-8）
Servo calibration match: YES      ← 标定数组与舵机 EEPROM 一致
IP: 192.168.x.x  RSSI: -xx        ← WiFi 正常
micro-ROS ready                   ← Agent 通信建立，RGB 变绿
```

上板风险点（代码无法离线验证的）：

- legacy I2S 驱动的 MCLK 输出（GPIO39，6.144MHz）——若无声/麦克风恒 0，改用 LEDC 生成 MCLK（备用方案已想好）；
- GC2145 JPEG 输出与画面方向；
- 1Mbps 舵机总线时序（GPIO19/20 与 CH340K 无关，独立验证）。

## 4. 与原版的差异速查

| 项目 | 原版 | 现在（NanoCam） |
|---|---|---|
| 舵机总线 | UART0，GPIO21/20 | UART1，GPIO20(TX)/19(RX)，接 P2-7/P2-8 |
| 调试串口 | USB CDC | UART0 → CH340K → USB-C（Linux: /dev/ttyUSB0） |
| 摄像头 | 无（Sense 版才有） | GC2145 DVP，MJPEG 流 |
| 音频 | 无 | ES8311 全双工 24kHz |
| RGB | 无 | WS2812 |
| Flash/PSRAM | 4MB/0 | 16MB/8MB Octal |

## 5. 尚未完成（含前序阶段延续）

1. **上板烧录与三行日志验证**（本节第 3 部分）；
2. 上板后提交"验证通过"版本到 git，作为已知可用回退点；
3. 通信失联的明显状态提示（RGB 已做 WiFi 丢失红色，可再评估）；
4. 5、10、20、30 米无线距离测试；
5. 无人机电机/电调 EMI 干扰测试（摄像头流与舵机总线在飞行时的稳定性）；
6. 无人机倒挂安装后的关节方向、重心、供电（BEC）方案；
7. 两人协同演示（一人无人机、一人主臂遥操）流程验证；
8. 比赛现场一页式操作说明。

## 6. 代码管理

- 本地 git 仓库：`/Users/Shared/SO-ARM&AmazingHand`（首次提交 c6b327a）。
- 远程：私有 GitHub 仓库。
- 日常提交推送：`git add -A && git commit -m "说明" && git push`。
- `wifi_config.h`（WiFi 密码）与 `.pio/` 构建缓存已在 .gitignore 中排除。
