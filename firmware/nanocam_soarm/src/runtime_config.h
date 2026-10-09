#pragma once

// 运行时串口配置 (WiFi SSID/密码/Agent IP)
//
// 优先级: NVS 保存的配置 > 编译期 wifi_config.h 默认值
// 串口命令 (115200, 通过调试口随时输入, 行以 \n 或 \r 结束):
//   wifi_ssid:<名称>   设置 WiFi SSID 并保存 (自动重启生效)
//   wifi_pass:<密码>   设置 WiFi 密码并保存 (自动重启生效)
//   agent_ip:<IP>      设置 micro-ROS Agent IP 并保存 (自动重启生效)
//   wifi_show          显示当前生效的配置
//   wifi_clear         清除保存的配置, 恢复编译期默认 (自动重启生效)
//
// 调用方式:
//   1. setup() 中在 WiFi.begin 之前调用 load_runtime_config() 覆盖默认值
//   2. 主循环中周期调用 handle_serial_commands() (非阻塞)

#include <stddef.h>

namespace runtime_config {

// 从 NVS 读取配置并复制到 out 缓冲区 (截断到 capacity-1)。
// 返回该字段是否在 NVS 中存在 (存在即覆盖编译期默认)。
bool load_ssid(char* out, size_t capacity);
bool load_password(char* out, size_t capacity);
bool load_agent_ip(char* out, size_t capacity);

// 非阻塞处理串口命令。返回 true 表示收到了需要重启才能生效的设置命令
// (调用方应延迟数秒打印提示后 ESP.restart())。
bool handle_serial_commands();

}  // namespace runtime_config
