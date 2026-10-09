#include "runtime_config.h"

#include <Arduino.h>
#include <Preferences.h>

namespace runtime_config {
namespace {

// NVS 命名空间与键名
constexpr const char* kNamespace = "wifi";
constexpr const char* kKeySsid = "ssid";
constexpr const char* kKeyPass = "pass";
constexpr const char* kKeyAgentIp = "agent_ip";

// 串口命令行缓冲 (最长 96 字节)
constexpr size_t kLineCapacity = 96;
char line[kLineCapacity];
size_t line_len = 0;

bool read_nvs(const char* key, char* out, size_t capacity) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        return false;
    }
    const bool found = prefs.isKey(key);
    if (found) {
        const size_t len = prefs.getString(key, out, capacity);
        if (len == 0) {
            prefs.end();
            return false;
        }
    }
    prefs.end();
    return found;
}

void write_nvs(const char* key, const char* value) {
    Preferences prefs;
    if (prefs.begin(kNamespace, false)) {
        prefs.putString(key, value);
        prefs.end();
    }
}

void clear_nvs() {
    Preferences prefs;
    if (prefs.begin(kNamespace, false)) {
        prefs.clear();
        prefs.end();
    }
}

// 去掉首尾空白
const char* trim(const char* s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

void print_current_config() {
    char ssid[64] = {0}, pass[64] = {0}, ip[32] = {0};
    const bool has_ssid = load_ssid(ssid, sizeof(ssid));
    const bool has_pass = load_password(pass, sizeof(pass));
    const bool has_ip = load_agent_ip(ip, sizeof(ip));
    Serial.printf("wifi_show: ssid=%s%s pass=%s%s agent_ip=%s%s\n",
        has_ssid ? ssid : "(默认)",
        has_ssid ? "" : " [未保存, 用编译期默认]",
        has_pass ? pass : "(默认)",
        has_pass ? "" : " [未保存, 用编译期默认]",
        has_ip ? ip : "(默认)",
        has_ip ? "" : " [未保存, 用编译期默认]");
}

}  // namespace

bool load_ssid(char* out, size_t capacity) {
    return read_nvs(kKeySsid, out, capacity);
}

bool load_password(char* out, size_t capacity) {
    return read_nvs(kKeyPass, out, capacity);
}

bool load_agent_ip(char* out, size_t capacity) {
    return read_nvs(kKeyAgentIp, out, capacity);
}

bool handle_serial_commands() {
    bool needs_reboot = false;

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (line_len == 0) {
                continue;
            }
            line[line_len] = '\0';
            line_len = 0;

            const char* cmd = trim(line);
            if (strncmp(cmd, "wifi_ssid:", 10) == 0) {
                const char* v = trim(cmd + 10);
                if (*v) {
                    write_nvs(kKeySsid, v);
                    Serial.printf("已保存 WiFi SSID: %s (3 秒后重启生效)\n", v);
                    needs_reboot = true;
                }
            } else if (strncmp(cmd, "wifi_pass:", 10) == 0) {
                const char* v = trim(cmd + 10);
                if (*v) {
                    write_nvs(kKeyPass, v);
                    Serial.printf("已保存 WiFi 密码 (3 秒后重启生效)\n");
                    needs_reboot = true;
                }
            } else if (strncmp(cmd, "agent_ip:", 9) == 0) {
                const char* v = trim(cmd + 9);
                if (*v) {
                    write_nvs(kKeyAgentIp, v);
                    Serial.printf("已保存 Agent IP: %s (3 秒后重启生效)\n", v);
                    needs_reboot = true;
                }
            } else if (strcmp(cmd, "wifi_show") == 0) {
                print_current_config();
            } else if (strcmp(cmd, "wifi_clear") == 0) {
                clear_nvs();
                Serial.println("已清除保存的 WiFi 配置, 恢复编译期默认 (3 秒后重启生效)");
                needs_reboot = true;
            } else {
                Serial.printf("未知命令: %s (可用: wifi_ssid: / wifi_pass: / agent_ip: / wifi_show / wifi_clear)\n",
                    cmd);
            }
        } else {
            if (line_len < kLineCapacity - 1) {
                line[line_len++] = c;
            }
            // 超长行: 丢弃直到换行
            else {
                line_len = kLineCapacity - 1;
            }
        }
    }
    return needs_reboot;
}

}  // namespace runtime_config
