#pragma once

#include <stdint.h>

// ESP32-NanoCam 板载音频: ES8311 编解码器 + AP2718AT 模拟麦克风 + NS4150B 功放
//   I2S (Master): MCLK=GPIO39  BCLK=GPIO38  WS=GPIO47  DIN(ADC)=GPIO40  DOUT(DAC)=GPIO48
//   I2C 控制:     SDA=GPIO41  SCL=GPIO42  (8-bit 地址 0x30, 与摄像头 SCCB 共用总线)
//   采样率:       24000 Hz, 16-bit, 立体声槽位 (与 NanoCam 原厂固件一致)
// 寄存器初始化序列移植自 Espressif esp_codec_dev 的 ES8311 驱动。

namespace audio_es8311 {

// 初始化 ES8311 + I2S 全双工通道。失败返回 false（不影响其余功能）。
bool init();

// 读取麦克风音量电平 (RMS, 归一化 0.0 ~ 1.0)。未初始化时返回 0。
float read_level();

// 播放一个正弦提示音 (阻塞, 时长较短)。
void play_tone(uint16_t freq_hz, uint16_t duration_ms);

// 启动提示音（两声短"嘀嘀"）。
void play_startup_chime();

// 控制解锁提示音（一声上扬音）。
void play_armed_chime();

// 错误提示音（一声低音）。
void play_error_chime();

}  // namespace audio_es8311
