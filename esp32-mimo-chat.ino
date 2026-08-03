/*
 * ESP32 MiMo Chat - Serial Chat Bot
 * Hardware: ESP32 Dev Module + 0.96" OLED + 4 Buttons
 * Author: Autumn
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============ Global Objects ============
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
WiFiClientSecure secureClient;  // Global to avoid heap corruption

// ============ State Variables ============
bool wifiConnected = false;
String inputBuffer = "";
unsigned long wifiCheckTime = 0;
String systemPrompt = "You are MiMo, an AI assistant by Xiaomi. Answer concisely in the same language as the user.";

// ============ SETUP ============
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println("\n================================\n  ESP32 MiMo Chat v1.0\n================================\n");

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 MiMo Chat");
  display.println("Connecting...");
  display.display();
  Serial.println("[OK] OLED ready");

  // Keys
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  Serial.println("[OK] Keys ready");

  // WiFi
  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);
  IPAddress local_IP(192, 168, 137, 100);
  IPAddress gateway(192, 168, 137, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(114, 114, 114, 114);
  WiFi.config(local_IP, gateway, subnet, dns1, dns2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.localIP().toString() == "0.0.0.0") {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  wifiConnected = true;
  Serial.print("[WIFI] Connected! IP: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.println("IP: " + WiFi.localIP().toString());
  display.println();
  display.println("MiMo Chat Ready!");
  display.println("Type message...");
  display.display();
  Serial.println("[OK] Ready. Type message:\n");
}

// ============ LOOP ============
void loop() {
  // Check WiFi every 5s
  if (millis() - wifiCheckTime > 5000) {
    wifiCheckTime = millis();
    bool connected = (WiFi.localIP().toString() != "0.0.0.0");
    if (!connected && wifiConnected) {
      wifiConnected = false;
      Serial.println("[WIFI] Lost!");
    }
  }

  // Read serial input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        if (inputBuffer.length() > 0) {
          Serial.print("\n[YOU] ");
          Serial.println(inputBuffer);
          
          if (!wifiConnected) {
            Serial.println("[ERR] WiFi not connected");
          } else {
            Serial.println("[...] Thinking...");
            String reply = callMiMoAPI(inputBuffer);
            if (reply.length() > 0) {
              Serial.print("\n[MIMO] ");
              Serial.println(reply);
              Serial.println();
            } else {
              Serial.println("[ERR] No response");
            }
          }
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

  // Key handler
  static unsigned long lastKeyTime = 0;
  if (millis() - lastKeyTime > 200) {
    if (digitalRead(KEY1_PIN) == LOW) {
      lastKeyTime = millis();
      Serial.println("\n[KEY1] Status:");
      Serial.print("  Heap: ");
      Serial.println(ESP.getFreeHeap());
    }
    if (digitalRead(KEY2_PIN) == LOW) {
      lastKeyTime = millis();
      Serial.println("\n[KEY2] Reconnect WiFi");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// ============ Build JSON (no ArduinoJson) ============
String buildRequestBody(const String& userInput) {
  String body = "{\"model\":\"";
  body += MIMO_MODEL;
  body += "\",\"max_completion_tokens\":";
  body += MAX_TOKENS;
  body += ",\"temperature\":1.0,\"stream\":false,\"thinking\":{\"type\":\"disabled\"},\"messages\":[{\"role\":\"system\",\"content\":\"";
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
  body += "\"}],\"tools\":[{\"type\":\"web_search\",\"max_keyword\":3,\"force_search\":true,\"limit\":1}],\"tool_choice\":\"auto\"";
  return body;
}

// ============ Call MiMo API ============
String callMiMoAPI(const String& userInput) {
  unsigned long startTime = millis();
  
  String requestBody = buildRequestBody(userInput);
  Serial.print("[REQ] ");
  Serial.print(requestBody.length());
  Serial.println(" bytes");

  secureClient.setInsecure();
  secureClient.setTimeout(30);

  if (!secureClient.connect("api.xiaomimimo.com", 443)) {
    Serial.println("[ERR] TCP failed");
    secureClient.stop();
    return "";
  }

  // Send HTTP request
  secureClient.print("POST /v1/chat/completions HTTP/1.1\r\nHost: api.xiaomimimo.com\r\nContent-Type: application/json\r\nAuthorization: Bearer ");
  secureClient.print(MIMO_API_KEY);
  secureClient.print("\r\nContent-Length: ");
  secureClient.print(requestBody.length());
  secureClient.print("\r\n\r\n");
  secureClient.print(requestBody);
  requestBody = "";

  // Wait for response
  unsigned long timeout = millis() + 30000;
  while (!secureClient.available() && millis() < timeout) delay(10);
  if (!secureClient.available()) {
    Serial.println("[ERR] Timeout");
    secureClient.stop();
    return "";
  }

  // Skip status line and headers
  String statusLine = secureClient.readStringUntil('\n');
  Serial.print("[REQ] ");
  Serial.println(statusLine);
  while (secureClient.available()) {
    String line = secureClient.readStringUntil('\n');
    if (line == "\r" || line.length() <= 1) break;
  }

  // Parse content inline
  String result = "";
  bool inContent = false, escaped = false;
  char search[] = "\"content\":\"";
  int searchIdx = 0;
  timeout = millis() + 10000;
  while (secureClient.available() && millis() < timeout) {
    char c = secureClient.read();
    if (!inContent) {
      if (c == search[searchIdx]) { searchIdx++; if (searchIdx >= 11) inContent = true; }
      else searchIdx = (c == search[0]) ? 1 : 0;
    } else {
      if (escaped) {
        if (c == 'n') result += '\n';
        else if (c == 'r') result += '\r';
        else if (c == '"') result += '"';
        else if (c == '\\') result += '\\';
        else result += c;
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        break;
      } else {
        result += c;
      }
    }
  }

  secureClient.stop();
  delay(100);
  yield();

  Serial.print("[REQ] Done in ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");

  return result;
}
