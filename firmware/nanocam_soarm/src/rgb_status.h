#pragma once

#include <stdint.h>

// ESP32-NanoCam 板载 WS2812 RGB LED (GPIO18, 1 颗, GRB)
// 使用 ESP-IDF legacy RMT 驱动, 无第三方依赖。

namespace rgb_status {

// 初始化 RMT TX 通道 (需在 WiFi/micro-ROS 启动前调用一次)
void init();

// 设置颜色 (r/g/b 0-255)。写入是异步的(RMT DMA), 不会阻塞主循环。
void set_color(uint8_t r, uint8_t g, uint8_t b);

// 熄灭
void off();

}  // namespace rgb_status
