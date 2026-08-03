/*
 * ESP32 MiMo Chat - Serial Chat Bot
 * Hardware: ESP32 Dev Module + 0.96" OLED + 4 Buttons
 * Author: Autumn
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// Disable brownout detector
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============ Global Objects ============
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ============ Chat History ============
struct Message {
  String role;
  String content;
};

Message history[MAX_HISTORY * 2 + 1];
int historyCount = 0;
String systemPrompt = "You are MiMo, an AI assistant by Xiaomi. Answer concisely in the same language as the user.";

// ============ State Variables ============
bool wifiConnected = false;
bool waitingResponse = false;
String inputBuffer = "";
unsigned long wifiCheckTime = 0;
int wifiStatus = WL_IDLE_STATUS;

// ============ Function Declarations ============
void startWiFi();
void checkWiFi();
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
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(SERIAL_BAUD);
  delay(500);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  ESP32 MiMo Chat v1.0");
  Serial.println("  Serial Chat Bot");
  Serial.println("================================");
  Serial.println();

  setupOLED();
  setupKeys();
  
  oledShowStatus("Connecting WiFi...");
  startWiFi();
  
  historyCount = 0;
  addToHistory("system", systemPrompt);
  
  Serial.println("[OK] System started");
  Serial.println("[OK] WiFi connecting in background...");
  Serial.println();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  checkWiFi();
  handleSerialInput();
  handleKeys();
}

// ============================================================
//  WiFi - Non-blocking
// ============================================================
void startWiFi() {
  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);
  
  // Set static IP + DNS to avoid DHCP/DNS issues
  IPAddress local_IP(192, 168, 137, 100);   // Static IP
  IPAddress gateway(192, 168, 137, 1);      // Gateway (Windows hotspot)
  IPAddress subnet(255, 255, 255, 0);       // Subnet mask
  IPAddress primaryDNS(8, 8, 8, 8);         // Google DNS
  IPAddress secondaryDNS(114, 114, 114, 114); // 114 DNS
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait until we get a valid IP address
  while (WiFi.localIP().toString() == "0.0.0.0")
  {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  wifiConnected = true;
  Serial.println("[WIFI] Connected!");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
  
  oledClear();
  oledShowLine(0, "WiFi Connected!");
  oledShowLine(1, "IP: " + WiFi.localIP().toString());
  oledShowLine(3, "MiMo Chat Ready!");
  oledShowLine(5, "Waiting input...");
  
  Serial.println("[OK] System ready. Type message:");
  Serial.println();
  
  // Debug: test TCP connection
  Serial.println("[TEST] Testing TCP to api.xiaomimimo.com:443...");
  WiFiClientSecure testClient;
  testClient.setInsecure();
  testClient.setTimeout(10);
  if (testClient.connect("api.xiaomimimo.com", 443)) {
    Serial.println("[TEST] TCP connected!");
    testClient.stop();
  } else {
    Serial.println("[TEST] TCP connection FAILED!");
  }
  
  // Debug: test DNS resolution
  Serial.println("[TEST] Resolving api.xiaomimimo.com...");
  IPAddress ip;
  if (WiFi.hostByName("api.xiaomimimo.com", ip)) {
    Serial.print("[TEST] Resolved to: ");
    Serial.println(ip);
  } else {
    Serial.println("[TEST] DNS resolution FAILED!");
  }
}

void checkWiFi() {
  // Only check every 5 seconds
  if (millis() - wifiCheckTime < 5000) return;
  wifiCheckTime = millis();
  
  // Use IP address to check connection (WiFi.status/isConnected buggy)
  bool connected = (WiFi.localIP().toString() != "0.0.0.0");
  
  if (connected && !wifiConnected) {
    wifiConnected = true;
    Serial.println("[WIFI] Reconnected!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
    oledClear();
    oledShowLine(0, "WiFi Connected!");
    oledShowLine(1, "IP: " + WiFi.localIP().toString());
    oledShowLine(3, "MiMo Chat Ready!");
    oledShowLine(5, "Waiting input...");
  } else if (!connected && wifiConnected) {
    wifiConnected = false;
    Serial.println("[WIFI] Lost!");
    oledShowStatus("WiFi Lost!");
  }
}

// ============================================================
//  OLED Setup
// ============================================================
void setupOLED() {
  Wire.begin(OLED_SDA, OLED_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERR] OLED init failed");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 MiMo Chat");
  display.println("Initializing...");
  display.display();
  
  Serial.println("[OK] OLED ready");
}

// ============================================================
//  Key Setup
// ============================================================
void setupKeys() {
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  
  Serial.println("[OK] Keys ready");
}

// ============================================================
//  OLED Display Functions
// ============================================================
void oledClear() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.display();
}

void oledShowLine(int line, const String& text) {
  if (line < 0 || line > 7) return;
  display.setTextSize(1);
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
  
  if (role == "user") {
    display.println("> You:");
  } else {
    display.println("> MiMo:");
  }
  
  display.println("Msg: " + String(text.length()) + " chars");
  display.println();
  display.println("Check Serial Monitor");
  display.println("for full content.");
  display.println();
  display.println("Baud: 115200");
  display.display();
}

// ============================================================
//  Serial Input Handler
// ============================================================
void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        
        if (inputBuffer.length() > 0) {
          if (!wifiConnected) {
            Serial.println("[ERR] WiFi not connected yet");
            inputBuffer = "";
            return;
          }
          
          Serial.println();
          Serial.print("[YOU] ");
          Serial.println(inputBuffer);
          oledShowChat("user", inputBuffer);
          
          waitingResponse = true;
          Serial.println("[...] Thinking...");
          oledShowStatus("Thinking...");
          
          String reply = callMiMoAPI(inputBuffer);
          
          if (reply.length() > 0) {
            Serial.println();
            Serial.print("[MIMO] ");
            Serial.println(reply);
            Serial.println();
            oledShowChat("assistant", reply);
            addToHistory("assistant", reply);
          } else {
            Serial.println("[ERR] No response");
            oledShowStatus("Request failed!");
          }
          
          waitingResponse = false;
          Serial.print("[YOU] ");
        }
        
        inputBuffer = "";
      }
    } else if (c == '\b' || c == 127) {
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.print("\b \b");
      }
    } else {
      inputBuffer += c;
      Serial.print(c);
    }
  }
}

// ============================================================
//  Key Handler
// ============================================================
void handleKeys() {
  static unsigned long lastKeyTime = 0;
  static bool lastKey1 = HIGH, lastKey2 = HIGH, lastKey3 = HIGH, lastKey4 = HIGH;
  
  if (millis() - lastKeyTime < 50) return;
  
  bool k1 = digitalRead(KEY1_PIN);
  bool k2 = digitalRead(KEY2_PIN);
  bool k3 = digitalRead(KEY3_PIN);
  bool k4 = digitalRead(KEY4_PIN);
  
  if (k1 == LOW && lastKey1 == HIGH) {
    lastKeyTime = millis();
    if (wifiConnected) {
      Serial.println("[KEY1] Send 'Hello'");
      inputBuffer = "Hello";
    } else {
      Serial.println("[KEY1] WiFi not ready");
    }
  }
  
  if (k2 == LOW && lastKey2 == HIGH) {
    lastKeyTime = millis();
    historyCount = 1;
    Serial.println("[KEY2] History cleared");
    oledShowStatus("History cleared");
    delay(500);
    oledShowLine(5, "Waiting input...");
  }
  
  if (k3 == LOW && lastKey3 == HIGH) {
    lastKeyTime = millis();
    Serial.println("[KEY3] Status:");
    Serial.print("  WiFi: ");
    Serial.println(wifiConnected ? "OK" : "FAIL");
    Serial.print("  Heap: ");
    Serial.println(ESP.getFreeHeap());
    
    oledClear();
    oledShowLine(0, "Status:");
    oledShowLine(1, "WiFi: " + String(wifiConnected ? "OK" : "FAIL"));
    oledShowLine(2, "Heap: " + String(ESP.getFreeHeap()));
    delay(2000);
    oledShowLine(5, "Waiting input...");
  }
  
  if (k4 == LOW && lastKey4 == HIGH) {
    lastKeyTime = millis();
    Serial.println("[KEY4] Reconnect WiFi");
    WiFi.disconnect();
    delay(100);
    startWiFi();
  }
  
  lastKey1 = k1;
  lastKey2 = k2;
  lastKey3 = k3;
  lastKey4 = k4;
}

// ============================================================
//  Chat History Management
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
  int keepFrom = historyCount - (MAX_HISTORY * 2);
  if (keepFrom < 1) keepFrom = 1;
  
  int newIndex = 1;
  for (int i = keepFrom; i < historyCount; i++) {
    history[newIndex] = history[i];
    newIndex++;
  }
  historyCount = newIndex;
}

// ============================================================
//  Build Request Body (手拼JSON，不用ArduinoJson省内存)
// ============================================================
String buildRequestBody(const String& userInput) {
  // 不做多轮对话，只发单条消息
  String body = "{\"model\":\"";
  body += MIMO_MODEL;
  body += "\",\"max_completion_tokens\":";
  body += MAX_TOKENS;
  body += ",\"temperature\":1.0,\"stream\":false,\"thinking\":{\"type\":\"disabled\"},\"messages\":[{\"role\":\"system\",\"content\":\"";
  // 简单转义引号
  for (unsigned int i = 0; i < systemPrompt.length(); i++) {
    if (systemPrompt[i] == '"') body += "\\\"";
    else body += systemPrompt[i];
  }
  body += "\"},{\"role\":\"user\",\"content\":\"";
  for (unsigned int i = 0; i < userInput.length(); i++) {
    if (userInput[i] == '"') body += "\\\"";
    else if (userInput[i] == '\\') body += "\\\\";
    else if (userInput[i] == '\n') body += "\\n";
    else if (userInput[i] == '\r') body += "\\r";
    else body += userInput[i];
  }
  body += "\"}]}";
  return body;
}

// ============================================================
//  Call MiMo API (边读边输出，最小内存)
// ============================================================
String callMiMoAPI(const String& userInput) {
  if (!wifiConnected) {
    Serial.println("[ERR] WiFi not connected");
    return "";
  }
  
  unsigned long startTime = millis();
  
  String requestBody = buildRequestBody(userInput);
  Serial.print("[REQ] Size: ");
  Serial.print(requestBody.length());
  Serial.println(" bytes");
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  
  if (!client.connect("api.xiaomimimo.com", 443)) {
    Serial.println("[ERR] TCP connect failed");
    client.stop();
    return "";
  }
  
  // 手写HTTP请求
  client.print("POST /v1/chat/completions HTTP/1.1\r\n");
  client.print("Host: api.xiaomimimo.com\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Authorization: Bearer ");
  client.print(MIMO_API_KEY);
  client.print("\r\n");
  client.print("Content-Length: ");
  client.print(requestBody.length());
  client.print("\r\n\r\n");
  client.print(requestBody);
  requestBody = ""; // 释放内存
  
  Serial.println("[REQ] Sent, waiting response...");
  
  // 等待响应
  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) {
    delay(10);
  }
  
  if (!client.available()) {
    Serial.println("[ERR] Response timeout");
    client.stop();
    return "";
  }
  
  // 读取HTTP状态行
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  Serial.print("[REQ] ");
  Serial.println(statusLine);
  
  // 跳过HTTP头
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() <= 1) break;
  }
  
  // 边读边解析：找 "content":" 然后直接输出到串口
  String result = "";
  bool inContent = false;
  bool escaped = false;
  char search[] = "\"content\":\"";
  int searchIdx = 0;
  int searchLen = 11;
  
  timeout = millis() + 10000;
  while (client.available() && millis() < timeout) {
    char c = client.read();
    
    if (!inContent) {
      // 搜索 "content":" 模式
      if (c == search[searchIdx]) {
        searchIdx++;
        if (searchIdx >= searchLen) {
          inContent = true;
        }
      } else {
        searchIdx = (c == search[0]) ? 1 : 0;
      }
    } else {
      // 在content内容中
      if (escaped) {
        if (c == 'n') result += '\n';
        else if (c == 'r') result += '\r';
        else if (c == 't') result += '\t';
        else if (c == '"') result += '"';
        else if (c == '\\') result += '\\';
        else result += c;
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        break; // 结束
      } else {
        result += c;
      }
    }
  }
  
  client.stop();
  delay(100);
  yield(); // let system reclaim TLS memory
  
  unsigned long elapsed = millis() - startTime;
  Serial.print("[REQ] Done in ");
  Serial.print(elapsed);
  Serial.println(" ms");
  
  return result;
}
