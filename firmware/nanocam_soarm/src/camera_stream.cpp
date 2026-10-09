// ESP32-NanoCam 板载 DVP 摄像头 MJPEG 流媒体服务
// 参考 Arduino-ESP32 core 3.x 官方示例 CameraWebServer 裁剪而来，
// 只保留单摄像头 + HTTP MJPEG 流，与 micro-ROS 遥操作共存。
#include "camera_stream.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <img_converters.h>

namespace camera_stream {
namespace {

// GC2145 无硬件 JPEG 编码, /stream 与 /jpg 经 frame2jpg 软件编码的质量
constexpr int kJpegQuality = 12;

// ESP32-NanoCam 摄像头 DVP 引脚（见 NanoCam 工程 main/nano_config.h）
// 默认传感器 GC2145 (FPC-24P 座, 兼容 OV2640/OV5640)
#define PWDN_GPIO_NUM 12
#define RESET_GPIO_NUM 14
#define XCLK_GPIO_NUM 9
#define SIOD_GPIO_NUM 41
#define SIOC_GPIO_NUM 42
#define Y9_GPIO_NUM 10
#define Y8_GPIO_NUM 8
#define Y7_GPIO_NUM 7
#define Y6_GPIO_NUM 5
#define Y5_GPIO_NUM 3
#define Y4_GPIO_NUM 1
#define Y3_GPIO_NUM 2
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 13
#define HREF_GPIO_NUM 11
#define PCLK_GPIO_NUM 6
#define NANOCAM_XCLK_FREQ 20000000

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;

esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    const char* html =
        "<html><body><h2>SO-ARM101 follower camera</h2>"
        "<img src=\"/stream\" style=\"width:640px\"><br>"
        "<a href=\"/jpg\">snapshot</a></body></html>";
    return httpd_resp_sendstr(req, html);
}

esp_err_t jpg_handler(httpd_req_t* req) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    uint8_t* jpg_buf = NULL;
    size_t jpg_len = 0;
    const bool ok = (fb->format == PIXFORMAT_RGB565) &&
        frame2jpg(fb, kJpegQuality, &jpg_buf, &jpg_len);
    if (!ok) {
        esp_camera_fb_return(fb);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    const esp_err_t res = httpd_resp_send(req, (const char*)jpg_buf, jpg_len);
    free(jpg_buf);
    esp_camera_fb_return(fb);
    return res;
}

// 阻塞式 MJPEG 流。运行在 httpd 任务中，发送慢时 TCP 背压自动降帧率，
// 不会拖慢主循环的 micro-ROS 遥操作。
esp_err_t stream_handler(httpd_req_t* req) {
    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // GC2145 输出 RGB565, 每帧经软件编码为 JPEG (frame2jpg) 后发送。
    uint8_t* jpg_buf = NULL;
    size_t jpg_len = 0;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
            break;
        }
        if (fb->format != PIXFORMAT_RGB565) {
            esp_camera_fb_return(fb);
            continue;
        }
        if (!frame2jpg(fb, kJpegQuality, &jpg_buf, &jpg_len)) {
            esp_camera_fb_return(fb);
            res = ESP_FAIL;
            break;
        }
        // 每帧之间发送 multipart 边界, 否则浏览器无法解析 MJPEG 流 (黑屏)。
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY,
            strlen(STREAM_BOUNDARY));
        if (res != ESP_OK) {
            free(jpg_buf);
            esp_camera_fb_return(fb);
            break;
        }
        const size_t hlen = snprintf(part_buf, sizeof(part_buf),
            STREAM_PART, static_cast<unsigned>(jpg_len));
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char*)jpg_buf, jpg_len);
        }
        free(jpg_buf);
        jpg_buf = NULL;
        esp_camera_fb_return(fb);
        if (res != ESP_OK) {
            break;
        }
    }
    httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    return res;
}

bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_2;
    config.ledc_timer = LEDC_TIMER_2;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = NANOCAM_XCLK_FREQ;
    // GC2145 无硬件 JPEG 编码器 (仅 OV2640/OV5640 有)。用 RGB565 采集,
    // /stream 与 /jpg 在发送前经 frame2jpg 软件编码为 JPEG。
    config.pixel_format = PIXFORMAT_RGB565;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    if (psramFound()) {
        // 正式配置: QVGA + 双缓冲。实测 VGA RGB565/YUV422 在此板 DVP 上
        // 数据率过高会花屏 (两个模块、XCLK 16/20/24MHz × 单/双缓冲均复现);
        // QVGA 完整流畅。双缓冲在软编码模式下让采集在编码期间继续, 更流畅。
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        // 无 PSRAM 兜底：QVGA 单缓冲
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("camera_stream: esp_camera_init failed: 0x%x\n", err);
        return false;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s != NULL) {
        Serial.printf("camera_stream: sensor PID 0x%02x\n", s->id.PID);
        if (s->id.PID == OV2640_PID) {
            s->set_vflip(s, 1);
        }
        s->set_framesize(s, FRAMESIZE_QVGA);
    }
    Serial.println("camera_stream: camera ready");
    return true;
}

bool start_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    // 默认栈 8KB 对 frame2jpg 软件编码太小, 会栈溢出导致 /jpg、/stream
    // 无响应 (浏览器一直转圈)。调到 16KB 容纳软编码栈。
    config.stack_size = 16384;

    if (httpd_start(&camera_httpd, &config) != ESP_OK) {
        Serial.println("camera_stream: httpd_start failed");
        return false;
    }

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET,
        .handler = index_handler, .user_ctx = NULL};
    httpd_uri_t jpg_uri = {.uri = "/jpg", .method = HTTP_GET,
        .handler = jpg_handler, .user_ctx = NULL};
    httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET,
        .handler = stream_handler, .user_ctx = NULL};

    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &jpg_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);

    Serial.printf("camera_stream: MJPEG server http://%s:81/stream\n",
        WiFi.localIP().toString().c_str());
    return true;
}

}  // namespace

bool start() {
    if (!init_camera()) {
        return false;
    }
    return start_server();
}

}  // namespace camera_stream
