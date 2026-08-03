#ifndef CONFIG_H
#define CONFIG_H

// ============ WiFi 配置 ============
#define WIFI_SSID     "Xiaomi15 By Autumn"
#define WIFI_PASSWORD "........"

// ============ MiMo API 配置 ============
#define MIMO_API_URL  "https://api.xiaomimimo.com/v1/chat/completions"
#define MIMO_API_KEY  "sk-ceel555ohvimi4u9jwt2p2ynah10dlf97t00df0wcxya3q5h"
#define MIMO_MODEL    "mimo-v2.5-pro"

// ============ 串口配置 ============
#define SERIAL_BAUD   115200

// ============ OLED 配置 ============
#define OLED_SDA      21
#define OLED_SCL      22
#define OLED_WIDTH    128
#define OLED_HEIGHT   64

// ============ 按键配置 ============
#define KEY1_PIN      25
#define KEY2_PIN      26
#define KEY3_PIN      32
#define KEY4_PIN      33

// ============ API 参数 ============
#define MAX_TOKENS    1024
#define TEMPERATURE   1.0
#define TOP_P         0.95

// ============ 对话历史 ============
#define MAX_HISTORY   4  // 保留最近N轮对话

#endif // CONFIG_H
