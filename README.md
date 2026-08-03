# ESP32 MiMo Chat

通过串口与小米MiMo大模型对话的ESP32聊天机器人。

## 硬件

- ESP32 Dev Module
- 0.96寸 OLED (IIC, SDA=21, SCL=22)
- 4个按键 (GPIO 25, 26, 32, 33)

## 功能

- 串口输入消息，调用MiMo API获取回答
- OLED显示对话内容
- 多轮对话支持（保留最近4轮）
- 按键操作：
  - KEY1 (GPIO25): 发送预设消息"你好"
  - KEY2 (GPIO26): 清除对话历史
  - KEY3 (GPIO32): 显示系统状态
  - KEY4 (GPIO33): 重新连接WiFi

## 编译

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 esp32-mimo-chat
```

## 烧录

```bash
arduino-cli upload -p COM_PORT --fqbn esp32:esp32:esp32 esp32-mimo-chat
```

## 使用

1. 修改 `config.h` 中的WiFi密码
2. 编译烧录
3. 打开串口监视器 (115200)
4. 输入消息，回车发送

## 依赖库

- Adafruit SSD1306
- Adafruit GFX Library
- ArduinoJson

## API

使用小米MiMo开放平台 OpenAI兼容接口：
- Endpoint: `https://api.xiaomimimo.com/v1/chat/completions`
- Model: `mimo-v2.5-pro`
