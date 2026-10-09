# SO-ARM101 无线遥操作项目进度（上板验证与问题修复）

日期：2026-08-19

> 前序阶段记录见 [soarm_project_progress_2026-08-15.md](./soarm_project_progress_2026-08-15.md)。
> 本次完成了 NanoCam 固件的首次完整上板验证，修复了 6 个真实问题，并完成遥操作全链路跑通。

## 1. 上板验证结论

NanoCam 固件首次完整上板，全部核心功能验证通过：

| 功能 | 状态 | 说明 |
|---|---|---|
| 音频 ES8311 | ✅ | `ES8311 ready @24000Hz`，扬声器提示音、麦克风电平均正常 |
| WiFi + micro-ROS | ✅ | `/joint_states` 20Hz 稳定，`/follower_audio/level` 正常 |
| 摄像头 FPV | ✅ | QVGA `/stream` 完整流畅 |
| 舵机总线 | ✅ | `Servo Ping mask: 0x3f`，`calibration match: YES` |
| 完整遥操作 | ✅ | 主臂跟随从臂，30Hz 命令，无跳变启动 |

## 2. 上板发现并修复的问题（6 个）

### 2.1 摄像头：I2C 冲突（i2c driver install error）

- **现象**：`i2c driver install error` → `sccb init err` → `camera probe failed`
- **根因**：ES8311 用 `Wire1`（GPIO41/42）占用 I2C 驱动；摄像头 SCCB 在同一对引脚
  再装 I2C 驱动被拒（"i2c driver install error"）。并非摄像头未插。
- **修复**：`audio_es8311.cpp` 的 `init()` 末尾加 `Wire1.end()`，ES8311 只在启动时
  用 I2C 配置，运行期只走 I2S，释放 GPIO41/42 给摄像头。

### 2.2 摄像头：GC2145 无硬件 JPEG（0x106）

- **现象**：`JPEG format is not supported on this sensor`，`esp_camera_init failed: 0x106`
- **根因**：GC2145 无内置 JPEG 编码器（仅 OV2640/OV5640 有）；固件配置 `PIXFORMAT_JPEG` 被拒。
- **修复**：采集改 `PIXFORMAT_RGB565`，`/stream` 与 `/jpg` 用 `frame2jpg` 软件编码。

### 2.3 摄像头：httpd 栈溢出（/jpg、/stream 无响应）

- **现象**：`/jpg` 返回 `HTTP:000`，浏览器一直转圈
- **根因**：`HTTPD_DEFAULT_CONFIG()` 默认栈 8KB，`frame2jpg` 软编码栈溢出拖死 httpd 任务。
- **修复**：`start_server()` 里 `config.stack_size = 16384`。

### 2.4 摄像头：/stream 黑屏（缺 multipart 边界）

- **现象**：`/stream` 能打开但全黑
- **根因**：每个 part 之间没发 `STREAM_BOUNDARY`，浏览器无法解析 multipart MJPEG。
- **修复**：每帧发送前补发 `STREAM_BOUNDARY`。

### 2.5 摄像头：VGA 花屏（硬件时序限制，非软件可解）

- **现象**：QVGA 完整；VGA(640×480) 画面下方约 2/3 花屏
- **排查**：VGA RGB565 在 XCLK 24/20/16MHz × 单/双缓冲共 4 种组合下均复现，QVGA 正常。
- **结论**：VGA RGB565 数据率超出此板 DVP 采样时序余量，属硬件限制。
- **处理**：正式配置用 **QVGA 320×240**（FPV 用途足够），双缓冲放 PSRAM。

### 2.6 音频：MCLK 缺失（扬声器无声 + 麦克风恒 0）

- **现象**：扬声器完全无声（含启动音），麦克风电平恒 ≈0（0.0009）
- **根因**：legacy I2S 驱动在 ESP32-S3 上不输出 MCLK（GPIO39 恒 0V），ES8311 内部
  DAC/ADC 无时钟，整条音频链路瘫。印证了 08-15 进度文档预判的风险点。
- **修复**：`audio_es8311.cpp` 新增 `start_ledc_mclk()`，用 **LEDC 在 GPIO39 生成
  6.154MHz MCLK**（80MHz÷13，误差 0.16% 在容差内），I2S 启动前调用。
- **附带**：`play_tone` 振幅 12000→30000、`R_DAC32` 0x30→0xFF，提示音从"贴耳才听到"
  提升到正常距离清晰（约 +29dB）。

## 3. 新增功能

### 3.1 Agent 失联自动重启（固件）

Ctrl+C 停掉 Agent 后，从臂原本只会停在已 armed 状态不重连。现固件主循环每 500ms
`rmw_uros_ping_agent` 探测 Agent，**连续失联超 10s 自动 `ESP.restart()`**，回到
`Waiting for micro-ROS Agent...`，可直接重跑遥操作脚本。

### 3.2 absolute 映射（PC 端）

`start_soarm_demo.sh` 的 `--mapping-mode` 由 `relative` 改为 **`absolute`**：

- **问题**：relative 模式在连接瞬间锁死"主臂原点 + 从臂原点"。断连重连后从臂停留在
  断连位置，主臂回零位后两边产生永久累积偏差。
- **解决**：absolute 使主臂/从臂姿态在各自标定坐标系一一对应。重连时从臂 8 秒内平滑
  对齐主臂当前姿态（`startup_blend`，无跳变），主臂回零 → 从臂也回自己零位。
- **验证**：断连重连后主臂回零，从臂同步回零，无偏差。✅

## 4. 调试经验（可复用）

- **摄像头排障顺序**：先看是否 `i2c driver install error`（I2C 冲突）→ 再看是否
  `JPEG not supported`（传感器无硬件 JPEG）→ 软编码后查 httpd 栈 → 流黑屏查 multipart
  边界 → 仍黑测 `/jpg` 单帧（能出图=链路通）→ 分辨率花屏多为 DVP 数据率硬件限制。
- **浏览器 vs curl**：esp_http_server 单任务，`/stream` 占用时 `/jpg` 会挂起；curl
  超时返回 `HTTP:000` 但浏览器能出图，优先用浏览器验证。
- **MCLK 是音频命脉**：ES8311 无声+麦克风恒 0 时先查 GPIO39 MCLK（应 ~1.65V）。
- **串口占用**：烧录前必须关掉 `pio device monitor`；残留 monitor 进程可用
  `Stop-Process` 清理，否则 upload 报 PermissionError。

## 5. 已知限制

- VGA 分辨率花屏为硬件时序限制，正式配置 QVGA。
- `/stream` 与 `/jpg` 不能同时访问（esp_http_server 单任务）。
- Agent 失联自动重启后从臂回零位（重新上电姿态），非保持断连位置。

## 6. 代码管理

- 本次改动涉及：`src/audio_es8311.cpp`、`src/camera_stream.cpp`、`src/main.cpp`、
  `start_soarm_demo.sh`，及同步更新的 `README.md`、`README_NANOCAM.md`、`完整运行教程.md`。
- 固件已上板验证通过，建议提交为可回退点。
