#include "rgb_status.h"

#include <Arduino.h>
#include <driver/rmt.h>

namespace rgb_status {
namespace {

// WS2812 时序 (RMT tick = 100ns @ clk_div 8)
//   0: 高 300ns + 低 800ns
//   1: 高 700ns + 低 600ns
// 帧间空闲低电平(>50us)即满足 RESET 时序
constexpr uint8_t kLedPin = 18;           // NANOCAM_RGB_LED_GPIO
constexpr rmt_channel_t kRmtChannel = RMT_CHANNEL_0;
constexpr uint8_t kLedCount = 1;
constexpr size_t kSymbolsPerLed = 24;     // GRB × 8bit

rmt_item32_t tx_items[kLedCount * kSymbolsPerLed] = {};

void append_bit(rmt_item32_t* items, size_t* index, bool bit) {
    if (bit) {
        items[*index].level0 = 1; items[*index].duration0 = 7;  // 700ns 高
        items[*index].level1 = 0; items[*index].duration1 = 6;  // 600ns 低
    } else {
        items[*index].level0 = 1; items[*index].duration0 = 3;  // 300ns 高
        items[*index].level1 = 0; items[*index].duration1 = 8;  // 800ns 低
    }
    (*index)++;
}

}  // namespace

void init() {
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = kRmtChannel;
    config.gpio_num = static_cast<gpio_num_t>(kLedPin);
    config.mem_block_num = 1;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.clk_div = 8;  // 80MHz / 8 = 10MHz → 100ns/tick
    if (rmt_config(&config) != ESP_OK ||
        rmt_driver_install(kRmtChannel, 0, 0) != ESP_OK) {
        Serial.println("rgb: RMT 初始化失败 (WS2812 状态灯不可用)");
        return;
    }
}

void set_color(uint8_t r, uint8_t g, uint8_t b) {
    // WS2812 数据顺序: GRB
    const uint8_t grb[kLedCount * 3] = {g, r, b};
    size_t index = 0;
    for (size_t led = 0; led < kLedCount; led++) {
        for (int byte = 0; byte < 3; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                append_bit(tx_items, &index,
                    (grb[led * 3 + byte] >> bit) & 0x01);
            }
        }
    }
    rmt_write_items(kRmtChannel, tx_items, index, false);
}

void off() {
    set_color(0, 0, 0);
}

}  // namespace rgb_status
