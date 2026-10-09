# NanoCam 新板子适配指南

> 如何将 NanoCam 固件移植到新的 ESP32-S3 硬件上。

## 1. 创建板级配置

```
main/boards/your_board/
├── config.h      # GPIO 引脚定义
├── config.json   # 板级元数据
└── board.cc      # 板级初始化代码
```

## 2. 定义引脚

参考 `main/boards/nano_cam/config.h`:

```c
// 摄像头 DVP
#define XCLK_GPIO_NUM   9     // 主时钟 24MHz
// D0-D7: 4,2,1,3,5,7,8,10
// VSYNC=13, HREF=11, PCLK=6, PWDN=12, RESET=14
// ... 完整引脚见硬件规格书

// I2S 音频 (ES8311 Standard Duplex)
#define AUDIO_I2S_GPIO_MCLK  39   // 主时钟
#define AUDIO_I2S_GPIO_BCLK  38   // 位时钟
#define AUDIO_I2S_GPIO_WS    47   // 字选择
#define AUDIO_I2S_GPIO_DOUT  48   // 数据输出
#define AUDIO_I2S_GPIO_DIN   40   // 数据输入
```

## 3. Kconfig 集成

在 `main/Kconfig.projbuild` 中添加:

```kconfig
choice NANOCAM_CAMERA_SENSOR
    config NANOCAM_CAMERA_YOUR_CAMERA
        bool "Your Camera Model"
endchoice
```

## 4. 编译测试

```bash
idf.py set-target esp32s3
idf.py menuconfig  # 选择你的板子配置
idf.py build flash monitor
```

## 5. 调试要点

- 串口日志: 波特率 115200 8N1
- 摄像头: 检查 `Camera init OK` 日志
- WiFi: 检查 `STA Got IP` 日志
- 内存: 检查 `Free Heap` 不低于 100KB
