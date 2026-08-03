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

// Display states
#define DISP_READY    0
#define DISP_THINKING 1
#define DISP_REPLYING 2
#define DISP_GPIO_OK  3
#define DISP_ERROR    4
int displayState = DISP_READY;

// ============ OLED Display ============
void updateDisplay() {
  display.clearDisplay();

  // Line 1: Title (large, centered)
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 0);
  display.println("ESP32 MiMo");

  // Line 2: WiFi status (small)
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.print("WiFi: ");
  display.println(wifiConnected ? "Connected" : "Offline");

  // Line 3: IP address (small)
  display.setCursor(0, 28);
  display.print("IP: ");
  display.println(WiFi.localIP().toString());

  // Separator line
  display.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // Status area
  display.setTextSize(1);
  const char* statusText = "";
  switch (displayState) {
    case DISP_READY:    statusText = ">> Ready <<"; break;
    case DISP_THINKING: statusText = "Thinking..."; break;
    case DISP_REPLYING: statusText = "Replying..."; break;
    case DISP_GPIO_OK:  statusText = "GPIO OK"; break;
    case DISP_ERROR:    statusText = "Error!"; break;
  }
  // Center status text
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(statusText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.println(statusText);

  display.display();
}
String systemPrompt = "You are MiMo, an AI assistant by Xiaomi with hardware control. Answer concisely in the same language as the user.\n\nWhen the user wants to control a GPIO pin, respond ONLY with JSON (no other text):\n{\"gpio\":PIN,\"state\":VALUE}\nPIN=pin number, VALUE=1(HIGH) or 0(LOW).\nAvailable: GPIO 2 (built-in LED, active LOW: 1=OFF, 0=ON).\nExamples:\nUser: turn on LED -> {\"gpio\":2,\"state\":0}\nUser: 把D2设为高电平 -> {\"gpio\":2,\"state\":1}\nFor normal questions, reply with text as usual.";

// ============ SETUP ============
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println("\n================================\n  ESP32 MiMo Chat v1.0\n================================\n");

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println("[OK] OLED ready");

  // Keys
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  Serial.println("[OK] Keys ready");

  // GPIO2 (built-in LED)
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);  // LED off (active LOW)
  Serial.println("[OK] GPIO2 ready");

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
  updateDisplay();  // Show "WiFi: Offline" while connecting
  while (WiFi.localIP().toString() == "0.0.0.0") {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  wifiConnected = true;
  Serial.print("[WIFI] Connected! IP: ");
  Serial.println(WiFi.localIP());

  displayState = DISP_READY;
  updateDisplay();
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
      updateDisplay();
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
            displayState = DISP_THINKING;
            updateDisplay();
            String reply = callMiMoAPI(inputBuffer);
            if (reply.length() > 0) {
              if (tryHandleGPIO(reply)) {
                Serial.println("[GPIO] Done");
                displayState = DISP_GPIO_OK;
              } else {
                Serial.print("\n[MIMO] ");
                Serial.println(reply);
                displayState = DISP_READY;
              }
              updateDisplay();
              Serial.println();
            } else {
              Serial.println("[ERR] No response");
              displayState = DISP_ERROR;
              updateDisplay();
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
  body += "\"}],\"tools\":[{\"type\":\"web_search\",\"max_keyword\":3,\"force_search\":true,\"limit\":1}],\"tool_choice\":\"auto\"}";
  return body;
}

// ============ GPIO Control ============
bool tryHandleGPIO(const String& reply) {
  String r = reply;
  r.trim();
  if (!r.startsWith("{")) return false;
  int gpioIdx = r.indexOf("\"gpio\":");
  int stateIdx = r.indexOf("\"state\":");
  if (gpioIdx < 0 || stateIdx < 0) return false;
  int pin = r.substring(gpioIdx + 7).toInt();
  int val = r.substring(stateIdx + 8).toInt();
  if (pin != 2) {
    Serial.print("[GPIO] Invalid pin: ");
    Serial.println(pin);
    return true;
  }
  digitalWrite(pin, val ? HIGH : LOW);
  Serial.print("[GPIO] D");
  Serial.print(pin);
  Serial.print(" -> ");
  Serial.println(val ? "HIGH" : "LOW");
  return true;
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

  // Buffer entire response body
  String responseBody = "";
  responseBody.reserve(2048);
  timeout = millis() + 15000;
  while (millis() < timeout) {
    while (secureClient.available()) {
      char c = secureClient.read();
      responseBody += c;
    }
    if (!secureClient.connected() && !secureClient.available()) break;
    delay(5);
  }
  secureClient.stop();
  Serial.print("[BUF] ");
  Serial.print(responseBody.length());
  Serial.println(" bytes");

  // Find "content":" in buffered response
  String result = "";
  const char* marker = "\"content\":\"";
  int idx = responseBody.indexOf(marker);
  if (idx >= 0) {
    int start = idx + 11; // length of "content":"
    bool escaped = false;
    for (int i = start; i < (int)responseBody.length(); i++) {
      char c = responseBody[i];
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

  Serial.print("[REQ] Done in ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");

  return result;
}
