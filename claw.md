# ESP32 MiMo Chat - 项目笔记

## 硬件配置
| 外设 | 引脚 | 说明 |
|------|------|------|
| OLED SDA | GPIO21 | IIC数据线 |
| OLED SCL | GPIO22 | IIC时钟线 |
| KEY1 | GPIO25 | 发送预设消息 |
| KEY2 | GPIO26 | 清除历史 |
| KEY3 | GPIO32 | 显示状态 |
| KEY4 | GPIO33 | 重连WiFi |

## API配置
- 平台: 小米MiMo开放平台
- 接口: OpenAI Chat Completions 兼容
- Endpoint: `https://api.xiaomimimo.com/v1/chat/completions`
- 模型: `mimo-v2.5-pro`
- 认证: `Authorization: Bearer API_KEY`

## 对话历史
- 保留最近4轮对话
- system prompt 不计入轮次
- 超出时自动裁剪旧消息

## 依赖库
- Adafruit SSD1306 (OLED驱动)
- Adafruit GFX Library (图形库)
- ArduinoJson (JSON解析)

## 已知限制
- HTTPS需要WiFiClientSecure，内存消耗较大
- 大模型回复过长时可能截断
- OLED显示空间有限，只显示部分内容
