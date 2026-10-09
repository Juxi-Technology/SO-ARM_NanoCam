#pragma once

// ============================================================
// ESP32-NanoCam 硬件引脚定义 (N16R8)
// 基于 Xingzhi Cube 1.54TFT WIFI 参考设计
// ============================================================

#include "driver/gpio.h"

// ---------- Camera DVP Interface (Xingzhi Cube reference) ----------
#define NANOCAM_CAMERA_MODULE_NAME   "NanoCam"

#define NANOCAM_CAM_PIN_PWDN    GPIO_NUM_12   // 摄像头断电控制
#define NANOCAM_CAM_PIN_RESET   GPIO_NUM_14   // 摄像头复位
#define NANOCAM_CAM_PIN_XCLK    GPIO_NUM_9    // 摄像头主时钟 24MHz
#define NANOCAM_CAM_PIN_SIOD    GPIO_NUM_41   // SCCB SDA (与 I2C 共享)
#define NANOCAM_CAM_PIN_SIOC    GPIO_NUM_42   // SCCB SCL (与 I2C 共享)

#define NANOCAM_CAM_PIN_D7      GPIO_NUM_10   // Y9
#define NANOCAM_CAM_PIN_D6      GPIO_NUM_8    // Y8
#define NANOCAM_CAM_PIN_D5      GPIO_NUM_7    // Y7
#define NANOCAM_CAM_PIN_D4      GPIO_NUM_5    // Y6
#define NANOCAM_CAM_PIN_D3      GPIO_NUM_3    // Y5
#define NANOCAM_CAM_PIN_D2      GPIO_NUM_1    // Y4
#define NANOCAM_CAM_PIN_D1      GPIO_NUM_2    // Y3
#define NANOCAM_CAM_PIN_D0      GPIO_NUM_4    // Y2

#define NANOCAM_CAM_PIN_VSYNC   GPIO_NUM_13   // 垂直同步
#define NANOCAM_CAM_PIN_HREF    GPIO_NUM_11   // 水平参考
#define NANOCAM_CAM_PIN_PCLK    GPIO_NUM_6    // 像素时钟

#define NANOCAM_CAM_XCLK_FREQ   24000000      // 24MHz

// ---------- Audio I2S (ES8311 Standard Duplex) ----------
// ES8311 Codec + AP2718AT Mic + NS4150B Amp
#define NANOCAM_ES8311_I2C_ADDR     0x30    // ES8311 I2C 地址 (8-bit)

#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_39   // I2S 主时钟
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_47   // I2S 字选择 (LRCK)
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_38   // I2S 位时钟 (SCLK)
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_40   // I2S 数据输入 (ADC)
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_48   // I2S 数据输出 (DAC)
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_NC   // 功放使能 (未使用)
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_41   // ES8311 I2C SDA
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_42   // ES8311 I2C SCL

// ---------- SPI Flash (GD25Q128ESIG) ----------
// 这些引脚由 ESP32-S3 模块内部固定，仅供参考
#define NANOCAM_FLASH_CS        GPIO_NUM_29
#define NANOCAM_FLASH_SCLK      GPIO_NUM_30
#define NANOCAM_FLASH_SO        GPIO_NUM_31
#define NANOCAM_FLASH_SI        GPIO_NUM_32
#define NANOCAM_FLASH_HOLD      GPIO_NUM_33
#define NANOCAM_FLASH_WP        GPIO_NUM_34

// ---------- I2C Bus (与摄像头 SCCB + ES8311 共用) ----------
#define NANOCAM_I2C_SDA         GPIO_NUM_41
#define NANOCAM_I2C_SCL         GPIO_NUM_42

// ---------- UART0 (USB Serial via CH340K) ----------
#define NANOCAM_UART0_TX        GPIO_NUM_43  // ESP32 TX → CH340K RXD
#define NANOCAM_UART0_RX        GPIO_NUM_44  // ESP32 RX ← CH340K TXD

// ---------- Expansion Serial ----------
#define NANOCAM_ESP_N           GPIO_NUM_19  // 扩展串口 RXD
#define NANOCAM_ESP_P           GPIO_NUM_20  // 扩展串口 TXD

// ---------- Boot & Control ----------
#define NANOCAM_BOOT_BUTTON     GPIO_NUM_0   // BOOT 按键
#define NANOCAM_CHIP_PU         GPIO_NUM_NC  // EN 引脚, 由底板 CH340K 一键下载电路控制

// ---------- WS2812 RGB LED ----------
#define NANOCAM_RGB_LED_GPIO    GPIO_NUM_18  // WS2812 数据引脚
#define NANOCAM_RGB_LED_COUNT   1            // WS2812 LED 数量

// ---------- Onboard LED (Power Indicator) ----------
#define NANOCAM_LED_GPIO        GPIO_NUM_NC  // 电源 LED 在底板上，核心板无直接控制

// ---------- SD Card / TF (if populated) ----------
#define NANOCAM_SD_DETECT       GPIO_NUM_NC  // 保留，未使用
