#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define BOARD_NAME "ESP32-NanoCam"

// ==================== Audio ES8311 (I2S Standard Duplex) ====================

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_39
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_47
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_38
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_48

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_41
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_42
#define AUDIO_CODEC_ES8311_ADDR  (0x30)

// ==================== Keys ====================

#define BOOT_BUTTON_GPIO        GPIO_NUM_0

// ==================== LED (WS2812 via RMT) ====================

#define BUILTIN_LED_GPIO        GPIO_NUM_18

// ==================== Camera (Xingzhi Cube reference) ====================

#define CAMERA_PIN_PWDN  GPIO_NUM_12
#define CAMERA_PIN_RESET GPIO_NUM_14
#define CAMERA_PIN_XCLK  GPIO_NUM_9
#define CAMERA_PIN_SIOD  GPIO_NUM_41
#define CAMERA_PIN_SIOC  GPIO_NUM_42

#define CAMERA_PIN_D7     GPIO_NUM_10
#define CAMERA_PIN_D6     GPIO_NUM_8
#define CAMERA_PIN_D5     GPIO_NUM_7
#define CAMERA_PIN_D4     GPIO_NUM_5
#define CAMERA_PIN_D3     GPIO_NUM_3
#define CAMERA_PIN_D2     GPIO_NUM_1
#define CAMERA_PIN_D1     GPIO_NUM_2
#define CAMERA_PIN_D0     GPIO_NUM_4
#define CAMERA_PIN_VSYNC  GPIO_NUM_13
#define CAMERA_PIN_HREF   GPIO_NUM_11
#define CAMERA_PIN_PCLK   GPIO_NUM_6

#define XCLK_FREQ_HZ 24000000

#endif  // _BOARD_CONFIG_H_
