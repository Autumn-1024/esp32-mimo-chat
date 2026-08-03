/*
 * ESP32 MiMo Chat - 串口聊天机器人
 * 功能：通过串口与小米MiMo大模型对话
 * 硬件：ESP32 Dev Module + 0.96寸OLED + 4按键
 * 作者：Autumn
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ============ 全局对象 ============
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ============ 对话历史 ============
struct Message {
  String role;
  String content;
};

Message history[MAX_HISTORY * 2 + 1]; // system + N轮(user+assistant)
int historyCount = 0;
String systemPrompt = "你是MiMo，小米AI助手。请用简洁的中文回答问题。";

// ============ 状态变量 ============
bool wifiConnected = false;
bool waitingResponse = false;
String inputBuffer = "";

// ============ 函数声明 ============
void setupWiFi();
void setupOLED();
void setupKeys();
void oledShowLine(int line, const String& text);
void oledClear();
void oledShowStatus(const String& status);
void oledShowChat(const String& role, const String& text);
String callMiMoAPI(const String& userInput);
String buildRequestBody(const String& userInput);
String extractContent(const String& json);
void addToHistory(const String& role, const String& content);
void trimHistory();
void handleSerialInput();
void handleKeys();

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  
  Serial.println("\n================================");
  Serial.println("  ESP32 MiMo Chat v1.0");
  Serial.println("  串口聊天机器人");
  Serial.println("================================\n");

  // 初始化硬件
  setupOLED();
  setupKeys();
  
  oledShowStatus("正在连接WiFi...");
  setupWiFi();
  
  if (wifiConnected) {
    oledShowStatus("WiFi已连接");
    delay(500);
    oledClear();
    oledShowLine(0, "MiMo Chat Ready!");
    oledShowLine(2, "等待输入...");
    
    Serial.println("✅ 系统就绪，请输入消息（回车发送）：\n");
  } else {
    oledShowStatus("WiFi连接失败!");
    Serial.println("❌ WiFi连接失败，请检查配置");
  }
  
  // 初始化对话历史
  historyCount = 0;
  addToHistory("system", systemPrompt);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  handleSerialInput();
  handleKeys();
}

// ============================================================
//  WiFi 初始化
// ============================================================
void setupWiFi() {
  Serial.print("📡 连接WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi已连接");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("   信号: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    wifiConnected = false;
    Serial.println("\n❌ WiFi连接超时");
  }
}

// ============================================================
//  OLED 初始化
// ============================================================
void setupOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED初始化失败");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 MiMo Chat");
  display.println("Initializing...");
  display.display();
  
  Serial.println("✅ OLED就绪 (SDA:" + String(OLED_SDA) + " SCL:" + String(OLED_SCL) + ")");
}

// ============================================================
//  按键初始化
// ============================================================
void setupKeys() {
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  
  Serial.println("✅ 按键就绪 (GPIO: " + String(KEY1_PIN) + "," + 
                 String(KEY2_PIN) + "," + String(KEY3_PIN) + "," + String(KEY4_PIN) + ")");
}

// ============================================================
//  OLED 显示函数
// ============================================================
void oledClear() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.display();
}

void oledShowLine(int line, const String& text) {
  if (line < 0 || line > 7) return; // 128x64, 8行
  display.setTextSize(1);
  
  // 清除该行区域
  display.fillRect(0, line * 8, OLED_WIDTH, 8, SSD1306_BLACK);
  display.setCursor(0, line * 8);
  display.println(text);
  display.display();
}

void oledShowStatus(const String& status) {
  oledClear();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("MiMo Chat");
  display.println("----------");
  display.println(status);
  display.display();
}

void oledShowChat(const String& role, const String& text) {
  oledClear();
  display.setCursor(0, 0);
  display.setTextSize(1);
  
  // 第一行显示角色
  if (role == "user") {
    display.println("> You:");
  } else {
    display.println("> MiMo:");
  }
  
  // 剩余行显示内容（自动截断）
  display.setTextSize(1);
  String displayText = text;
  // 简单截断，每行约21个字符，共7行
  int maxChars = 21 * 7;
  if (displayText.length() > maxChars) {
    displayText = displayText.substring(0, maxChars - 3) + "...";
  }
  display.println(displayText);
  display.display();
}

// ============================================================
//  串口输入处理
// ============================================================
void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        
        if (inputBuffer.length() > 0) {
          Serial.println("\n👤 You: " + inputBuffer);
          oledShowChat("user", inputBuffer);
          
          // 调用API
          waitingResponse = true;
          Serial.println("⏳ MiMo正在思考...\n");
          oledShowStatus("MiMo思考中...");
          
          String reply = callMiMoAPI(inputBuffer);
          
          if (reply.length() > 0) {
            Serial.println("🤖 MiMo: " + reply + "\n");
            oledShowChat("assistant", reply);
            addToHistory("assistant", reply);
          } else {
            Serial.println("❌ 获取回复失败\n");
            oledShowStatus("请求失败!");
          }
          
          waitingResponse = false;
          Serial.println("请输入消息（回车发送）：");
        }
        
        inputBuffer = "";
      }
    } else if (c == '\b' || c == 127) { // 退格
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.print("\b \b");
      }
    } else {
      inputBuffer += c;
      Serial.print(c); // 回显
    }
  }
}

// ============================================================
//  按键处理
// ============================================================
void handleKeys() {
  static unsigned long lastKeyTime = 0;
  static bool lastKey1 = HIGH, lastKey2 = HIGH, lastKey3 = HIGH, lastKey4 = HIGH;
  
  if (millis() - lastKeyTime < 50) return; // 消抖
  
  bool k1 = digitalRead(KEY1_PIN);
  bool k2 = digitalRead(KEY2_PIN);
  bool k3 = digitalRead(KEY3_PIN);
  bool k4 = digitalRead(KEY4_PIN);
  
  // 按键1 - 发送预设消息
  if (k1 == LOW && lastKey1 == HIGH) {
    lastKeyTime = millis();
    Serial.println("\n🔘 KEY1: 发送 '你好'");
    inputBuffer = "你好";
    Serial.println(inputBuffer);
  }
  
  // 按键2 - 清除历史
  if (k2 == LOW && lastKey2 == HIGH) {
    lastKeyTime = millis();
    historyCount = 1; // 只保留system prompt
    Serial.println("\n🔘 KEY2: 对话历史已清除");
    oledShowStatus("历史已清除");
    delay(500);
    oledShowLine(2, "等待输入...");
  }
  
  // 按键3 - 显示状态
  if (k3 == LOW && lastKey3 == HIGH) {
    lastKeyTime = millis();
    Serial.println("\n🔘 KEY3: 显示状态");
    Serial.println("  WiFi: " + String(wifiConnected ? "已连接" : "断开"));
    Serial.println("  历史: " + String(historyCount) + " 条");
    Serial.println("  内存: " + String(ESP.getFreeHeap()) + " bytes");
    
    oledClear();
    oledShowLine(0, "Status:");
    oledShowLine(1, "WiFi: " + String(wifiConnected ? "OK" : "FAIL"));
    oledShowLine(2, "History: " + String(historyCount));
    oledShowLine(3, "Heap: " + String(ESP.getFreeHeap()));
    delay(2000);
    oledShowLine(2, "等待输入...");
  }
  
  // 按键4 - 重新连接WiFi
  if (k4 == LOW && lastKey4 == HIGH) {
    lastKeyTime = millis();
    Serial.println("\n🔘 KEY4: 重连WiFi");
    oledShowStatus("重连WiFi...");
    setupWiFi();
    if (wifiConnected) {
      oledShowStatus("WiFi已连接");
    } else {
      oledShowStatus("WiFi连接失败!");
    }
    delay(1000);
    oledShowLine(2, "等待输入...");
  }
  
  lastKey1 = k1;
  lastKey2 = k2;
  lastKey3 = k3;
  lastKey4 = k4;
}

// ============================================================
//  对话历史管理
// ============================================================
void addToHistory(const String& role, const String& content) {
  if (historyCount >= MAX_HISTORY * 2 + 1) {
    trimHistory();
  }
  history[historyCount].role = role;
  history[historyCount].content = content;
  historyCount++;
}

void trimHistory() {
  // 保留 system prompt (index 0) + 最近的对话
  int keepFrom = historyCount - (MAX_HISTORY * 2);
  if (keepFrom < 1) keepFrom = 1;
  
  // 移动保留的消息
  int newIndex = 1;
  for (int i = keepFrom; i < historyCount; i++) {
    history[newIndex] = history[i];
    newIndex++;
  }
  historyCount = newIndex;
}

// ============================================================
//  构建请求体
// ============================================================
String buildRequestBody(const String& userInput) {
  // 添加用户消息到历史
  addToHistory("user", userInput);
  
  // 构建JSON
  // 使用 StaticJsonDocument 节省内存
  // 如果内存不够，可以降低 MAX_HISTORY
  DynamicJsonDocument doc(8192);
  
  doc["model"] = MIMO_MODEL;
  doc["max_completion_tokens"] = MAX_TOKENS;
  doc["temperature"] = TEMPERATURE;
  doc["top_p"] = TOP_P;
  doc["stream"] = false;
  
  JsonObject thinking = doc.createNestedObject("thinking");
  thinking["type"] = "disabled";
  
  JsonArray messages = doc.createNestedArray("messages");
  for (int i = 0; i < historyCount; i++) {
    JsonObject msg = messages.createNestedObject();
    msg["role"] = history[i].role;
    msg["content"] = history[i].content;
  }
  
  String body;
  serializeJson(doc, body);
  return body;
}

// ============================================================
//  从JSON提取content
// ============================================================
String extractContent(const String& json) {
  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    Serial.println("❌ JSON解析失败: " + String(error.c_str()));
    return "";
  }
  
  // 检查是否有错误
  if (doc.containsKey("error")) {
    String errorMsg = doc["error"]["message"].as<String>();
    Serial.println("❌ API错误: " + errorMsg);
    return "错误: " + errorMsg;
  }
  
  // 提取 content
  const char* content = doc["choices"][0]["message"]["content"];
  if (content) {
    return String(content);
  }
  
  return "";
}

// ============================================================
//  调用 MiMo API
// ============================================================
String callMiMoAPI(const String& userInput) {
  if (!wifiConnected) {
    Serial.println("❌ WiFi未连接");
    return "";
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("❌ WiFi已断开");
    return "";
  }
  
  unsigned long startTime = millis();
  
  String requestBody = buildRequestBody(userInput);
  
  // 调试：打印请求大小
  Serial.println("📦 请求体大小: " + String(requestBody.length()) + " bytes");
  
  WiFiClientSecure client;
  client.setInsecure(); // 跳过证书验证（开发阶段）
  client.setTimeout(30); // 30秒超时
  
  HTTPClient http;
  http.begin(client, MIMO_API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(MIMO_API_KEY));
  http.setTimeout(30000); // 30秒超时
  
  Serial.println("📡 发送请求到 MiMo API...");
  
  int httpCode = http.POST(requestBody);
  
  String response = "";
  if (httpCode > 0) {
    Serial.println("📨 HTTP状态码: " + String(httpCode));
    
    if (httpCode == HTTP_CODE_OK) {
      response = http.getString();
      String content = extractContent(response);
      
      unsigned long elapsed = millis() - startTime;
      Serial.println("⏱️ 耗时: " + String(elapsed) + "ms");
      
      http.end();
      client.stop();
      return content;
    } else {
      Serial.println("❌ HTTP错误: " + http.errorToString(httpCode));
      response = http.getString();
      Serial.println("   响应: " + response);
    }
  } else {
    Serial.println("❌ 请求失败: " + http.errorToString(httpCode));
  }
  
  http.end();
  client.stop();
  return "";
}
