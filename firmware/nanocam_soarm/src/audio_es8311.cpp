#include "audio_es8311.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <driver/ledc.h>
#include <math.h>

namespace audio_es8311 {
namespace {

// ---------- 引脚 (NanoCam) ----------
constexpr uint8_t kI2sMclkPin = 39;
constexpr uint8_t kI2sBclkPin = 38;
constexpr uint8_t kI2sWsPin = 47;
constexpr uint8_t kI2sDinPin = 40;   // ADC 数据 (麦克风 → ESP32)
constexpr uint8_t kI2sDoutPin = 48;  // DAC 数据 (ESP32 → 扬声器)
constexpr uint8_t kI2cSdaPin = 41;
constexpr uint8_t kI2cSclPin = 42;
constexpr uint8_t kEs8311Addr = 0x18;  // 0x30 (8-bit) → 7-bit

constexpr int kSampleRate = 24000;
constexpr i2s_port_t kI2sPort = I2S_NUM_0;

// ES8311 寄存器 (见 es8311_reg.h)
constexpr uint8_t R_RESET = 0x00;
constexpr uint8_t R_CLK1 = 0x01;
constexpr uint8_t R_CLK2 = 0x02;
constexpr uint8_t R_CLK3 = 0x03;
constexpr uint8_t R_CLK4 = 0x04;
constexpr uint8_t R_CLK5 = 0x05;
constexpr uint8_t R_CLK6 = 0x06;
constexpr uint8_t R_CLK7 = 0x07;
constexpr uint8_t R_CLK8 = 0x08;
constexpr uint8_t R_SDPIN = 0x09;
constexpr uint8_t R_SDPOUT = 0x0A;
constexpr uint8_t R_SYS0B = 0x0B;
constexpr uint8_t R_SYS0C = 0x0C;
constexpr uint8_t R_SYS0D = 0x0D;
constexpr uint8_t R_SYS0E = 0x0E;
constexpr uint8_t R_SYS10 = 0x10;
constexpr uint8_t R_SYS11 = 0x11;
constexpr uint8_t R_SYS12 = 0x12;
constexpr uint8_t R_SYS13 = 0x13;
constexpr uint8_t R_SYS14 = 0x14;
constexpr uint8_t R_ADC15 = 0x15;
constexpr uint8_t R_ADC16 = 0x16;
constexpr uint8_t R_ADC17 = 0x17;
constexpr uint8_t R_ADC1B = 0x1B;
constexpr uint8_t R_ADC1C = 0x1C;
constexpr uint8_t R_DAC31 = 0x31;  // DAC mute
constexpr uint8_t R_DAC32 = 0x32;  // DAC volume
constexpr uint8_t R_DAC37 = 0x37;
constexpr uint8_t R_GPIO44 = 0x44;
constexpr uint8_t R_GP45 = 0x45;

bool initialized = false;
bool i2s_enabled = false;

bool es8311_write(uint8_t reg, uint8_t value) {
    Wire1.beginTransmission(kEs8311Addr);
    Wire1.write(reg);
    Wire1.write(value);
    return Wire1.endTransmission() == 0;
}

uint8_t es8311_read(uint8_t reg) {
    Wire1.beginTransmission(kEs8311Addr);
    Wire1.write(reg);
    if (Wire1.endTransmission(false) != 0) {
        return 0xFF;
    }
    if (Wire1.requestFrom((int)kEs8311Addr, 1) != 1) {
        return 0xFF;
    }
    return Wire1.read();
}

// 依次写入一组 (reg, value)，全部成功返回 true
bool es8311_write_seq(const uint8_t seq[][2], size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!es8311_write(seq[i][0], seq[i][1])) {
            return false;
        }
    }
    return true;
}

bool es8311_init() {
    // 与 esp_codec_dev es8311_open() 相同的初始化序列 (slave 模式, use_mclk)
    const uint8_t seq1[][2] = {
        {R_SYS0D, 0xFA},   // power up
        {R_GPIO44, 0x08},  // I2C noise immunity (写两次确保可靠)
        {R_GPIO44, 0x08},
        {R_CLK1, 0x30},
        {R_CLK2, 0x00},
        {R_CLK3, 0x10},
        {R_ADC16, 0x24},   // ADC 配置 (MIC gain scale)
        {R_CLK4, 0x10},
        {R_CLK5, 0x00},
        {R_SYS0B, 0x00},
        {R_SYS0C, 0x00},
        {R_SYS10, 0x1F},
        {R_SYS11, 0x7F},
        {R_RESET, 0x80},   // 数字复位
    };
    if (!es8311_write_seq(seq1, sizeof(seq1) / sizeof(seq1[0]))) {
        return false;
    }
    // slave 模式: 清 master 位 (0x40)
    uint8_t regv = es8311_read(R_RESET) & 0xBF;
    if (!es8311_write(R_RESET, regv)) {
        return false;
    }
    // 时钟源: use_mclk, 不反相
    if (!es8311_write(R_CLK1, 0x3F)) {
        return false;
    }
    // SCLK 不反相
    regv = es8311_read(R_CLK6) & ~0x20;
    if (!es8311_write(R_CLK6, regv)) {
        return false;
    }
    const uint8_t seq2[][2] = {
        {R_SYS13, 0x10},
        {R_ADC1B, 0x0A},
        {R_ADC1C, 0x6A},
        {R_GPIO44, 0x58},  // 内部参考 (ADCL + DACR), 模拟麦克风
    };
    if (!es8311_write_seq(seq2, sizeof(seq2) / sizeof(seq2[0]))) {
        return false;
    }

    // 串口格式: I2S Philips 标准 + 16bit
    if (!es8311_write(R_SDPIN, 0x0C) || !es8311_write(R_SDPOUT, 0x0C)) {
        return false;
    }

    // 时钟分频: 24000Hz @ MCLK=6.144MHz (256×FS), 系数表 {6144000, 24000, 0x01,0x01,0x01,0x01,0x00,0x00,0xff,0x04,0x10,0x10}
    const uint8_t seq3[][2] = {
        {R_CLK2, 0x00},  // pre_div=1, pre_multi=x1
        {R_CLK5, 0x00},  // adc_div=1, dac_div=1
        {R_CLK3, 0x10},  // adc fs_mode=ss, adc_osr=16
        {R_CLK4, 0x10},  // dac_osr=16
        {R_CLK7, 0x00},  // lrck_h
        {R_CLK8, 0xFF},  // lrck_l
        {R_CLK6, 0x03},  // bclk_div=4 → 3
    };
    if (!es8311_write_seq(seq3, sizeof(seq3) / sizeof(seq3[0]))) {
        return false;
    }
    return true;
}

// ES8311 需要外部 MCLK (6.144MHz @ 24kHz) 才能让内部 DAC/ADC 时钟工作。
// 实测 legacy I2S 驱动在 ESP32-S3 上不输出 MCLK (GPIO39 恒 0V), 导致扬声器
// 完全无声、麦克风电平 ≈ 0。改用 LEDC 在 GPIO39 上独立产生方波 MCLK:
//   80MHz APB ÷ 13 ≈ 6.154MHz, 误差 0.16%, 在 ES8311 时钟容差内。
constexpr ledc_timer_t kMclkLedcTimer = LEDC_TIMER_3;
constexpr ledc_channel_t kMclkLedcChannel = LEDC_CHANNEL_3;

void start_ledc_mclk() {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num = kMclkLedcTimer;
    timer.duty_resolution = LEDC_TIMER_1_BIT;  // 50% 方波
    timer.freq_hz = 80000000 / 13;
    timer.clk_cfg = LEDC_USE_APB_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        Serial.println("audio: LEDC MCLK timer config failed");
        return;
    }
    ledc_channel_config_t channel = {};
    channel.gpio_num = kI2sMclkPin;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = kMclkLedcChannel;
    channel.timer_sel = kMclkLedcTimer;
    channel.duty = 1;  // 1-bit 分辨率: duty=1 → 50%
    channel.hpoint = 0;
    if (ledc_channel_config(&channel) != ESP_OK) {
        Serial.println("audio: LEDC MCLK channel config failed");
    }
}

bool es8311_start() {
    // 与 es8311_start() 相同 (codec_mode = BOTH)
    const uint8_t seq[][2] = {
        {R_RESET, 0x80},  // slave 模式启动
        {R_SDPIN, 0x0C},  // 清 bit6, 保持 I2S/16bit
        {R_SDPOUT, 0x0C},
        {R_ADC17, 0xBF},  // ADC 音量
        {R_SYS0E, 0x02},  // 上电
        {R_SYS12, 0x00},  // 使能 DAC
        {R_SYS14, 0x1A},  // 模拟 PGA (非数字麦克风)
        {R_SYS0D, 0x01},  // power up
        {R_ADC15, 0x40},
        {R_DAC37, 0x08},  // DAC ramp rate
        {R_GP45, 0x00},
        {R_DAC31, 0x00},  // 解除静音
        {R_DAC32, 0xFF},  // DAC 音量最大 (默认 0x30 太小声, 贴耳才能听到)
    };
    return es8311_write_seq(seq, sizeof(seq) / sizeof(seq[0]));
}

bool i2s_init() {
    i2s_config_t cfg = {};
    cfg.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    cfg.sample_rate = kSampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;  // 6.144MHz @ 24kHz

    esp_err_t err = i2s_driver_install(kI2sPort, &cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("audio: i2s_driver_install failed: 0x%x\n", err);
        return false;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = kI2sMclkPin;
    pins.bck_io_num = kI2sBclkPin;
    pins.ws_io_num = kI2sWsPin;
    pins.data_out_num = kI2sDoutPin;
    pins.data_in_num = kI2sDinPin;
    err = i2s_set_pin(kI2sPort, &pins);
    if (err != ESP_OK) {
        Serial.printf("audio: i2s_set_pin failed: 0x%x\n", err);
        return false;
    }
    return true;
}

}  // namespace

bool init() {
    if (initialized) {
        return true;
    }
    // ES8311 控制走 I2C1 (Wire1)；物理总线 GPIO41/42 与摄像头 SCCB 共用,
    // 摄像头只在启动时使用 SCCB, 运行期无冲突。
    Wire1.begin(kI2cSdaPin, kI2cSclPin, 400000);

    if (!es8311_init()) {
        Serial.println("audio: ES8311 I2C 配置失败 (检查 41/42 与上拉)");
        return false;
    }
    // ES8311 的 DAC/ADC 依赖外部 MCLK。必须在 I2S 启动前用 LEDC 输出,
    // 否则 codec 内部时钟不工作 (扬声器无声 + 麦克风电平恒 0)。
    start_ledc_mclk();
    if (!i2s_init()) {
        return false;
    }
    if (!es8311_start()) {
        Serial.println("audio: ES8311 启动失败");
        return false;
    }
    i2s_zero_dma_buffer(kI2sPort);
    // ES8311 只在启动时用 I2C 配置; 运行期 read_level/play_tone 只用 I2S。
    // 必须在此释放 I2C 驱动, 否则后续摄像头 SCCB 在同一对 GPIO41/42 上
    // i2c_driver_install 会失败 ("i2c driver install error")。
    Wire1.end();
    initialized = true;
    i2s_enabled = true;
    Serial.println("audio: ES8311 ready @24000Hz");
    return true;
}

float read_level() {
    if (!i2s_enabled) {
        return 0.0f;
    }
    // 一次读 128 帧 (256 samples, 立体声); 只用左声道
    static int16_t buf[128 * 2];
    size_t bytes_read = 0;
    // 超时必须短 (10ms): 本函数在主循环里每 200ms 调用一次,
    // 若 ES8311 未锁定导致 RX 无数据, 过长的阻塞会拖慢 20Hz 关节发布。
    esp_err_t err = i2s_read(kI2sPort, buf, sizeof(buf), &bytes_read,
        pdMS_TO_TICKS(10));
    if (err != ESP_OK || bytes_read == 0) {
        return 0.0f;
    }
    const size_t frames = bytes_read / 4;
    double sum = 0;
    for (size_t i = 0; i < frames; i++) {
        const double s = buf[i * 2] / 32768.0;
        sum += s * s;
    }
    const double rms = sqrt(sum / frames);
    // 归一化到 0..1 (线性压缩一点, 让环境噪声可区分)
    return static_cast<float>(rms < 1.0 ? rms : 1.0);
}

void play_tone(uint16_t freq_hz, uint16_t duration_ms) {
    if (!i2s_enabled) {
        return;
    }
    static int16_t chunk[512 * 2];  // 512 帧立体声
    const size_t frames_per_chunk = 512;
    const size_t total_frames = static_cast<size_t>(kSampleRate)
        * duration_ms / 1000;
    const double phase_step = 2.0 * M_PI * freq_hz / kSampleRate;
    double phase = 0.0;
    size_t remaining = total_frames;
    while (remaining > 0) {
        const size_t n = remaining < frames_per_chunk ? remaining : frames_per_chunk;
        for (size_t i = 0; i < n; i++) {
            const int16_t sample = static_cast<int16_t>(
                sin(phase) * 30000.0);
            chunk[i * 2] = sample;
            chunk[i * 2 + 1] = sample;
            phase += phase_step;
            if (phase >= 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
        size_t written = 0;
        i2s_write(kI2sPort, chunk, n * 4, &written, pdMS_TO_TICKS(100));
        remaining -= n;
    }
}

void play_startup_chime() {
    play_tone(880, 70);
    delay(60);
    play_tone(1320, 120);
}

void play_armed_chime() {
    play_tone(660, 80);
    delay(50);
    play_tone(990, 140);
}

void play_error_chime() {
    play_tone(220, 200);
}

}  // namespace audio_es8311
