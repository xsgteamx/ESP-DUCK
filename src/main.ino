#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

// ===================== User Config =====================
const char* WEB_PASSWORD = "CHANGE_ME_WEB_PASSWORD";        // 本地 Wi-Fi 网页和 /api/message token
const char* SETUP_AP_SSID = "ESP-DUCK";
const char* SETUP_AP_PASS = "CHANGE_ME_12345678";     // 至少 8 位
const char* DEVICE_NAME = "ESP-DUCK";
const char* FIRMWARE_VERSION = "2026-06-03-stable-usb-settings-v3";

// ===================== Objects =====================
WebServer server(80);
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
Preferences prefs;

// ===================== Wi-Fi State =====================
String savedWifiSsid = "";
String savedWifiPass = "";
bool setupPortalActive = false;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

// ===================== HID Typing Config =====================
const int DEFAULT_CHAR_DELAY_MS = 70;
const int DEFAULT_RISK_CHAR_DELAY_MS = 140;
const int DEFAULT_LINE_DELAY_MS = 400;
const int DEFAULT_LINE_START_DELAY_MS = 150;
const int DEFAULT_CHUNK_SIZE = 24;
const int DEFAULT_CHUNK_DELAY_MS = 120;
const int DEFAULT_BEFORE_TYPE_DELAY_MS = 5000;

int charDelayMs = DEFAULT_CHAR_DELAY_MS;
int riskCharDelayMs = DEFAULT_RISK_CHAR_DELAY_MS;
int lineDelayMs = DEFAULT_LINE_DELAY_MS;
int lineStartDelayMs = DEFAULT_LINE_START_DELAY_MS;
int chunkSize = DEFAULT_CHUNK_SIZE;
int chunkDelayMs = DEFAULT_CHUNK_DELAY_MS;
int beforeTypeDelayMs = DEFAULT_BEFORE_TYPE_DELAY_MS;

// ===================== Keep Alive Config =====================
const bool DEFAULT_KEEP_ALIVE_ENABLED = false;
const unsigned long DEFAULT_KEEP_ALIVE_INTERVAL_MS = 180000;
const char* DEFAULT_KEEP_ALIVE_ACTION = "click_f5"; // mouse_move / mouse_click / f5 / click_f5 / click_f5_loop

bool keepAliveEnabled = DEFAULT_KEEP_ALIVE_ENABLED;
unsigned long keepAliveIntervalMs = DEFAULT_KEEP_ALIVE_INTERVAL_MS;
unsigned long lastKeepAliveAt = 0;
bool typingActive = false;
String keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;

// ===================== Reset Button Config =====================
// 默认使用大多数 ESP32-S3 开发板的 BOOT 键 GPIO0。短按：软重启；长按 5 秒：清空 Wi-Fi 和应用设置后重启。
const int RESET_BUTTON_PIN = 0;
const unsigned long RESET_BUTTON_DEBOUNCE_MS = 40;
const unsigned long RESET_BUTTON_LONG_PRESS_MS = 5000;
bool resetButtonDown = false;
bool resetButtonLongHandled = false;
unsigned long resetButtonDownAt = 0;

// ===================== UART Protocol State =====================
const size_t SERIAL_MAX_MESSAGE_LEN = 60000;
const unsigned long SERIAL_RX_TIMEOUT_MS = 10000;

enum SerialRxState {
  SERIAL_WAIT_HEADER,
  SERIAL_READ_BODY
};

SerialRxState serialRxState = SERIAL_WAIT_HEADER;
String serialHeaderLine = "";
String serialJsonBody = "";
size_t serialExpectedLen = 0;
unsigned long serialLastByteAt = 0;

// ===================== HTML Pages =====================
const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32-HID 登录</title><style>
body{margin:0;padding:28px 14px;background:#f5f7fb;color:#1f2937;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei",Arial,sans-serif}.card{max-width:420px;margin:80px auto;background:#fff;border:1px solid #e5e7eb;border-radius:20px;padding:24px;box-shadow:0 10px 30px rgba(15,23,42,.08)}h1{margin:0 0 12px;font-size:24px}p{color:#6b7280;line-height:1.7;font-size:14px}input{width:100%;border:1px solid #e5e7eb;border-radius:12px;padding:12px;font-size:16px;outline:none;box-sizing:border-box}button{width:100%;margin-top:14px;border:none;border-radius:12px;padding:12px;font-size:16px;color:white;background:#2563eb;cursor:pointer}</style></head><body><div class="card"><h1>ESP32-HID 输入器</h1><p>请输入访问密码。</p><form method="POST" action="/login"><input name="password" type="password" placeholder="访问密码" autofocus><button type="submit">进入</button></form></div></body></html>
)rawliteral";

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32-HID Wi-Fi 配置</title><style>
body{margin:0;padding:20px 14px;background:#f5f7fb;color:#1f2937;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei",Arial,sans-serif}.card{max-width:720px;margin:0 auto;background:#fff;border:1px solid #e5e7eb;border-radius:20px;padding:22px;box-shadow:0 10px 30px rgba(15,23,42,.08)}h1{margin:0 0 8px;font-size:24px}p{color:#6b7280;line-height:1.7;font-size:14px}.field{margin-top:12px}label{display:block;color:#6b7280;font-size:13px;margin-bottom:6px}input{width:100%;border:1px solid #e5e7eb;border-radius:12px;padding:11px;font-size:15px;box-sizing:border-box}button,a{display:inline-block;margin-top:14px;border:none;border-radius:12px;padding:11px 16px;font-size:15px;color:white;background:#2563eb;text-decoration:none;cursor:pointer}.secondary{background:#eef2f7;color:#1f2937}.danger{background:#dc2626;color:white}.hint{margin-top:14px;color:#6b7280;font-size:13px;line-height:1.8;background:#fafafa;border:1px dashed #e5e7eb;border-radius:12px;padding:12px}</style></head><body><div class="card"><h1>ESP32-HID Wi-Fi 配置</h1><p>开发板当前处于配置热点模式。保存 Wi-Fi 后，串口会输出新 IP。</p><form method="POST" action="/portal_save"><div class="field"><label>SSID</label><input name="ssid" placeholder="输入 Wi-Fi 名称"></div><div class="field"><label>Wi-Fi 密码</label><input name="password" type="password" placeholder="输入 Wi-Fi 密码"></div><button type="submit">保存并连接</button><a class="secondary" href="/portal_scan">扫描 Wi-Fi</a><a class="danger" href="/portal_clear">清空配置</a></form><div class="hint">配置热点：ESP-DUCK<br>默认热点密码：CHANGE_ME_12345678<br>ESP32-S3 仅支持 2.4GHz Wi-Fi。</div></div></body></html>
)rawliteral";

// INDEX_HTML moved to LittleFS: /index.html


// ===================== Forward Declarations =====================
bool isValidKeepAliveAction(const String& action);
void setRuntimeDefaults();
void loadAppSettings();
void saveAppSettings();
void clearAppSettingsOnly();
void factoryResetAllSettings();
void handleResetButton();
void delayWithButtonCheck(unsigned long ms);

// ===================== Helpers =====================
int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int readIntArg(const char* name, int defaultValue, int minValue, int maxValue) {
  if (!server.hasArg(name)) return defaultValue;
  return clampInt(server.arg(name).toInt(), minValue, maxValue);
}

bool isAuthenticated() {
  if (!server.hasHeader("Cookie")) return false;
  String cookie = server.header("Cookie");
  return cookie.indexOf("ESP32_AUTH=1") >= 0;
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void sendHttpJson(const String& json) {
  addCorsHeaders();
  server.send(200, "application/json; charset=utf-8", json);
}

void sendHttpOptions() {
  addCorsHeaders();
  server.send(204);
}

String makeJsonOk(const String& msg) {
  JsonDocument doc;
  doc["ok"] = true;
  doc["msg"] = msg;
  String out;
  serializeJson(doc, out);
  return out;
}

String makeJsonError(const String& msg) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["msg"] = msg;
  String out;
  serializeJson(doc, out);
  return out;
}

void sendSerialError(const String& msg) {
  Serial.print("{\"ok\":false,\"msg\":\"");
  Serial.print(msg);
  Serial.println("\"}");
}

// ===================== Wi-Fi =====================
void loadWifiConfig() {
  prefs.begin("wifi", true);
  savedWifiSsid = prefs.getString("ssid", "");
  savedWifiPass = prefs.getString("pass", "");
  prefs.end();
  Serial.print("Saved Wi-Fi SSID: ");
  Serial.println(savedWifiSsid.length() ? savedWifiSsid : "(empty)");
}

void saveWifiConfig(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  savedWifiSsid = ssid;
  savedWifiPass = pass;
}

void clearWifiConfig() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  savedWifiSsid = "";
  savedWifiPass = "";
}

// ===================== App Settings Persistence =====================
void setRuntimeDefaults() {
  charDelayMs = DEFAULT_CHAR_DELAY_MS;
  riskCharDelayMs = DEFAULT_RISK_CHAR_DELAY_MS;
  lineDelayMs = DEFAULT_LINE_DELAY_MS;
  lineStartDelayMs = DEFAULT_LINE_START_DELAY_MS;
  chunkSize = DEFAULT_CHUNK_SIZE;
  chunkDelayMs = DEFAULT_CHUNK_DELAY_MS;
  beforeTypeDelayMs = DEFAULT_BEFORE_TYPE_DELAY_MS;
  keepAliveEnabled = DEFAULT_KEEP_ALIVE_ENABLED;
  keepAliveIntervalMs = DEFAULT_KEEP_ALIVE_INTERVAL_MS;
  keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;
}

void loadAppSettings() {
  prefs.begin("app", true);
  charDelayMs = clampInt(prefs.getInt("charDelay", DEFAULT_CHAR_DELAY_MS), 1, 800);
  riskCharDelayMs = clampInt(prefs.getInt("riskDelay", DEFAULT_RISK_CHAR_DELAY_MS), 1, 1000);
  lineDelayMs = clampInt(prefs.getInt("lineDelay", DEFAULT_LINE_DELAY_MS), 0, 2000);
  lineStartDelayMs = clampInt(prefs.getInt("lineStart", DEFAULT_LINE_START_DELAY_MS), 0, 1000);
  chunkSize = clampInt(prefs.getInt("chunkSize", DEFAULT_CHUNK_SIZE), 1, 240);
  chunkDelayMs = clampInt(prefs.getInt("chunkDelay", DEFAULT_CHUNK_DELAY_MS), 0, 1000);
  beforeTypeDelayMs = clampInt(prefs.getInt("startDelay", DEFAULT_BEFORE_TYPE_DELAY_MS), 1000, 30000);
  keepAliveEnabled = prefs.getBool("kaEnabled", DEFAULT_KEEP_ALIVE_ENABLED);
  unsigned long intervalSeconds = prefs.getULong("kaInterval", DEFAULT_KEEP_ALIVE_INTERVAL_MS / 1000UL);
  intervalSeconds = (unsigned long)clampInt((int)intervalSeconds, 10, 86400);
  keepAliveIntervalMs = intervalSeconds * 1000UL;
  keepAliveAction = prefs.getString("kaAction", DEFAULT_KEEP_ALIVE_ACTION);
  prefs.end();
  if (!isValidKeepAliveAction(keepAliveAction)) keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;
}

void saveAppSettings() {
  if (!isValidKeepAliveAction(keepAliveAction)) keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;
  prefs.begin("app", false);
  prefs.putInt("charDelay", charDelayMs);
  prefs.putInt("riskDelay", riskCharDelayMs);
  prefs.putInt("lineDelay", lineDelayMs);
  prefs.putInt("lineStart", lineStartDelayMs);
  prefs.putInt("chunkSize", chunkSize);
  prefs.putInt("chunkDelay", chunkDelayMs);
  prefs.putInt("startDelay", beforeTypeDelayMs);
  prefs.putBool("kaEnabled", keepAliveEnabled);
  prefs.putULong("kaInterval", keepAliveIntervalMs / 1000UL);
  prefs.putString("kaAction", keepAliveAction);
  prefs.end();
}

void clearAppSettingsOnly() {
  prefs.begin("app", false);
  prefs.clear();
  prefs.end();
  setRuntimeDefaults();
}

void factoryResetAllSettings() {
  clearAppSettingsOnly();
  clearWifiConfig();
}

void handleResetButton() {
  bool down = digitalRead(RESET_BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (down && !resetButtonDown) {
    resetButtonDown = true;
    resetButtonLongHandled = false;
    resetButtonDownAt = now;
    return;
  }

  if (down && resetButtonDown && !resetButtonLongHandled && now - resetButtonDownAt >= RESET_BUTTON_LONG_PRESS_MS) {
    resetButtonLongHandled = true;
    factoryResetAllSettings();
    Serial.println("Button long press: factory reset all settings.");
    delay(200);
    ESP.restart();
    return;
  }

  if (!down && resetButtonDown) {
    unsigned long held = now - resetButtonDownAt;
    resetButtonDown = false;
    if (!resetButtonLongHandled && held >= RESET_BUTTON_DEBOUNCE_MS) {
      Serial.println("Button short press: restart device.");
      delayWithButtonCheck(120);
      ESP.restart();
    }
  }
}

void delayWithButtonCheck(unsigned long ms) {
  unsigned long startAt = millis();
  while (millis() - startAt < ms) {
    handleResetButton();
    delay(10);
  }
}

void startSetupPortal() {
  setupPortalActive = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS);
  Serial.println("Setup portal started.");
  Serial.print("AP SSID: ");
  Serial.println(SETUP_AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

bool connectToWifi(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(300);
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startAt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startAt > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("\nWi-Fi connect timeout.");
      return false;
    }
  }
  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectSavedWifi() {
  loadWifiConfig();
  if (!savedWifiSsid.length()) {
    startSetupPortal();
    return false;
  }
  bool ok = connectToWifi(savedWifiSsid, savedWifiPass);
  if (!ok) startSetupPortal();
  else setupPortalActive = false;
  return ok;
}

String handleWifiScanCommand() {
  Serial.println("Scanning Wi-Fi...");
  WiFi.mode(setupPortalActive ? WIFI_AP_STA : WIFI_STA);
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  doc["ok"] = true;
  doc["type"] = "wifi_scan";
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject item = arr.add<JsonObject>();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["channel"] = WiFi.channel(i);
    item["encrypted"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  String out;
  serializeJson(doc, out);
  Serial.print("Wi-Fi scan done, count = ");
  Serial.println(n);
  return out;
}

String handleWifiStatusCommand() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["type"] = "wifi_status";
  doc["connected"] = WiFi.status() == WL_CONNECTED;
  doc["ssid"] = WiFi.SSID();
  doc["savedSsid"] = savedWifiSsid;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["setupPortalActive"] = setupPortalActive;
  doc["setupApSsid"] = setupPortalActive ? SETUP_AP_SSID : "";
  String out;
  serializeJson(doc, out);
  return out;
}

String handleWifiSaveCommand(JsonDocument& doc) {
  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  if (!ssid.length()) return makeJsonError("wifi_ssid_empty");
  saveWifiConfig(ssid, password);
  bool connected = connectToWifi(ssid, password);
  if (!connected) startSetupPortal();
  else setupPortalActive = false;
  JsonDocument res;
  res["ok"] = connected;
  res["type"] = "wifi_save";
  res["connected"] = connected;
  res["ssid"] = ssid;
  res["ip"] = connected ? WiFi.localIP().toString() : "";
  res["msg"] = connected ? "wifi_connected" : "wifi_connect_failed";
  String out;
  serializeJson(res, out);
  return out;
}

String handleWifiClearCommand() {
  clearWifiConfig();
  WiFi.disconnect(true);
  startSetupPortal();
  JsonDocument doc;
  doc["ok"] = true;
  doc["type"] = "wifi_clear";
  doc["msg"] = "wifi_config_cleared";
  String out;
  serializeJson(doc, out);
  return out;
}

// ===================== HID =====================
bool isRiskChar(char c) {
  return c == '"' || c == '\'' || c == '(' || c == ')' ||
         c == '{' || c == '}' || c == '[' || c == ']' ||
         c == '<' || c == '>' || c == ':' || c == ';' ||
         c == '+' || c == '-' || c == '_' || c == '=' ||
         c == '!' || c == '@' || c == '#' || c == '$' ||
         c == '%' || c == '^' || c == '&' || c == '*' ||
         c == '\\' || c == '/' || c == '|' || c == '`' ||
         c == '~' || c == ',' || c == '.';
}

void applyTypingConfigFromJson(JsonObject config) {
  if (config.isNull()) return;
  charDelayMs = clampInt(config["charDelay"] | charDelayMs, 1, 800);
  riskCharDelayMs = clampInt(config["riskDelay"] | riskCharDelayMs, 1, 1000);
  lineDelayMs = clampInt(config["lineDelay"] | lineDelayMs, 0, 2000);
  lineStartDelayMs = clampInt(config["lineStartDelay"] | lineStartDelayMs, 0, 1000);
  chunkSize = clampInt(config["chunkSize"] | chunkSize, 1, 240);
  chunkDelayMs = clampInt(config["chunkDelay"] | chunkDelayMs, 0, 1000);
  beforeTypeDelayMs = clampInt(config["startDelay"] | beforeTypeDelayMs, 1000, 30000);
}

void typeText(const String& text) {
  delayWithButtonCheck(300);
  bool atLineStart = true;
  int charsInChunk = 0;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\r') continue;
    if (c == '\n') {
      Keyboard.write(KEY_RETURN);
      delayWithButtonCheck(lineDelayMs);
      atLineStart = true;
      charsInChunk = 0;
      continue;
    }
    if (atLineStart) {
      delayWithButtonCheck(lineStartDelayMs);
      atLineStart = false;
    }
    if (c == '\t') {
      Keyboard.write(KEY_TAB);
      delayWithButtonCheck(riskCharDelayMs);
      charsInChunk++;
    } else {
      Keyboard.write((uint8_t)c);
      charsInChunk++;
      if (isRiskChar(c)) delayWithButtonCheck(riskCharDelayMs);
      else if (c == ' ') delayWithButtonCheck(charDelayMs + 30);
      else delayWithButtonCheck(charDelayMs);
    }
    if (chunkSize > 0 && charsInChunk >= chunkSize) {
      delayWithButtonCheck(chunkDelayMs);
      charsInChunk = 0;
    }
  }
  delayWithButtonCheck(300);
}

bool isValidKeepAliveAction(const String& action) {
  return action == "mouse_move" ||
         action == "mouse_click" ||
         action == "f5" ||
         action == "click_f5" ||
         action == "click_f5_loop";
}

void keepAliveShiftOnce() {
  Keyboard.press(KEY_LEFT_SHIFT);
  delayWithButtonCheck(80);
  Keyboard.release(KEY_LEFT_SHIFT);
  delayWithButtonCheck(80);
}

void keepAliveMouseMoveOnce() {
  Mouse.move(1, 0);
  delayWithButtonCheck(80);
  Mouse.move(-1, 0);
  delayWithButtonCheck(80);
}

void keepAliveMouseClickOnce() {
  Mouse.click(MOUSE_LEFT);
  delayWithButtonCheck(120);
}

void keepAliveF5Once() {
  Keyboard.press(KEY_F5);
  delayWithButtonCheck(100);
  Keyboard.release(KEY_F5);
  delayWithButtonCheck(120);
}

void keepAliveClickAndF5Once() {
  keepAliveMouseClickOnce();
  keepAliveF5Once();
}

void sendKeepAlive() {
  Serial.print("Keep alive action: ");
  Serial.println(keepAliveAction);

  if (keepAliveAction == "mouse_move") {
    keepAliveMouseMoveOnce();
    return;
  }

  if (keepAliveAction == "mouse_click") {
    keepAliveMouseClickOnce();
    return;
  }

  if (keepAliveAction == "f5") {
    keepAliveF5Once();
    return;
  }

  if (keepAliveAction == "click_f5") {
    keepAliveClickAndF5Once();
    return;
  }

  if (keepAliveAction == "click_f5_loop") {
    // 一次保活动作内执行 10 轮：鼠标左键点击 1 次 + F5 1 次；每轮间隔约 1 秒
    for (int i = 0; i < 10; i++) {
      keepAliveClickAndF5Once();
      if (i < 9) {
        delayWithButtonCheck(1000);
      }
    }
    return;
  }

  keepAliveClickAndF5Once();
}

// ===================== JSON Command =====================
String handleJsonCommand(JsonDocument& doc, bool allowTyping) {
  String type = doc["type"] | "";

  if (type == "status") {
    JsonDocument res;
    res["ok"] = true;
    res["type"] = "status";
    res["deviceName"] = DEVICE_NAME;
    res["firmware"] = FIRMWARE_VERSION;
    res["mac"] = WiFi.macAddress();
    res["keepAliveEnabled"] = keepAliveEnabled;
    res["keepAliveInterval"] = keepAliveIntervalMs / 1000UL;
    res["keepAliveAction"] = keepAliveAction;
    res["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    res["wifiSsid"] = WiFi.SSID();
    res["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    res["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    res["typingActive"] = typingActive;
    res["setupPortalActive"] = setupPortalActive;
    res["charDelay"] = charDelayMs;
    res["riskDelay"] = riskCharDelayMs;
    res["lineDelay"] = lineDelayMs;
    res["lineStartDelay"] = lineStartDelayMs;
    res["chunkSize"] = chunkSize;
    res["chunkDelay"] = chunkDelayMs;
    res["startDelay"] = beforeTypeDelayMs;
    String out;
    serializeJson(res, out);
    return out;
  }

  if (type == "settings") {
    keepAliveEnabled = doc["keepAliveEnabled"] | keepAliveEnabled;
    int intervalSeconds = doc["keepAliveInterval"] | (int)(keepAliveIntervalMs / 1000UL);
    intervalSeconds = clampInt(intervalSeconds, 10, 86400);
    keepAliveIntervalMs = (unsigned long)intervalSeconds * 1000UL;
    String action = doc["keepAliveAction"] | keepAliveAction;
    if (isValidKeepAliveAction(action)) keepAliveAction = action;
    else keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;
    saveAppSettings();
    lastKeepAliveAt = millis();
    JsonDocument res;
    res["ok"] = true;
    res["type"] = "settings";
    res["msg"] = "settings_updated";
    res["keepAliveEnabled"] = keepAliveEnabled;
    res["keepAliveInterval"] = keepAliveIntervalMs / 1000UL;
    res["keepAliveAction"] = keepAliveAction;
    String out;
    serializeJson(res, out);
    return out;
  }

  if (type == "wifi_scan") return handleWifiScanCommand();
  if (type == "wifi_status") return handleWifiStatusCommand();
  if (type == "wifi_save") return handleWifiSaveCommand(doc);
  if (type == "wifi_clear") return handleWifiClearCommand();
  if (type == "reboot") return makeJsonOk("rebooting");

  if (type == "type") {
    if (!allowTyping) return makeJsonError("typing_not_allowed");
    String text = doc["text"] | "";
    if (!text.length()) return makeJsonError("empty_text");
    JsonObject config = doc["config"].as<JsonObject>();
    applyTypingConfigFromJson(config);
    saveAppSettings();
    delayWithButtonCheck(beforeTypeDelayMs);
    typingActive = true;
    Serial.println("Typing start...");
    typeText(text);
    Serial.println("Typing done.");
    typingActive = false;
    lastKeepAliveAt = millis();
    return makeJsonOk("type_done");
  }

  return makeJsonError("unknown_type");
}

// ===================== UART Protocol =====================
void resetSerialReceiver() {
  serialRxState = SERIAL_WAIT_HEADER;
  serialHeaderLine = "";
  serialJsonBody = "";
  serialExpectedLen = 0;
}

void processSerialJsonMessage(const String& payload) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    sendSerialError("json_parse_failed");
    return;
  }
  String result = handleJsonCommand(doc, true);
  Serial.println(result);
  String type = doc["type"] | "";
  if (type == "reboot") {
    delay(300);
    ESP.restart();
  }
}

void handleSerialInput() {
  if (serialRxState == SERIAL_READ_BODY && millis() - serialLastByteAt > SERIAL_RX_TIMEOUT_MS) {
    sendSerialError("serial_rx_timeout");
    resetSerialReceiver();
  }

  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    serialLastByteAt = millis();

    if (serialRxState == SERIAL_WAIT_HEADER) {
      if (ch == '\r') continue;
      if (ch == '\n') {
        if (serialHeaderLine.startsWith("ESPMSG:")) {
          int len = serialHeaderLine.substring(7).toInt();
          if (len <= 0 || len > (int)SERIAL_MAX_MESSAGE_LEN) {
            sendSerialError("invalid_length");
            resetSerialReceiver();
            return;
          }
          serialExpectedLen = (size_t)len;
          serialJsonBody = "";
          serialJsonBody.reserve(serialExpectedLen + 1);
          serialRxState = SERIAL_READ_BODY;
          Serial.print("{\"ok\":true,\"msg\":\"ready\",\"len\":");
          Serial.print(serialExpectedLen);
          Serial.println("}");
        } else {
          sendSerialError("invalid_header");
          resetSerialReceiver();
        }
        serialHeaderLine = "";
        continue;
      }
      if (serialHeaderLine.length() < 80) serialHeaderLine += ch;
      else {
        sendSerialError("header_too_long");
        resetSerialReceiver();
        return;
      }
      continue;
    }

    if (serialRxState == SERIAL_READ_BODY) {
      serialJsonBody += ch;
      if (serialJsonBody.length() >= serialExpectedLen) {
        String payload = serialJsonBody;
        resetSerialReceiver();
        processSerialJsonMessage(payload);
        return;
      }
    }
  }
}


bool streamLittleFsFile(const char* path, const char* contentType) {
  if (!LittleFS.exists(path)) {
    Serial.print("LittleFS missing file: ");
    Serial.println(path);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.print("LittleFS open failed: ");
    Serial.println(path);
    return false;
  }

  server.streamFile(file, contentType);
  file.close();
  return true;
}

// ===================== HTTP Routes =====================
void sendLoginPage() {
  server.send(200, "text/html; charset=utf-8", LOGIN_HTML);
}

void handleRoot() {
  if (setupPortalActive && WiFi.status() != WL_CONNECTED) {
    server.send(200, "text/html; charset=utf-8", PORTAL_HTML);
    return;
  }

  if (!isAuthenticated()) {
    sendLoginPage();
    return;
  }

  if (!streamLittleFsFile("/index.html", "text/html; charset=utf-8")) {
    server.send(500, "text/plain; charset=utf-8", "LittleFS /index.html not found. Upload data/index.html first.");
  }
}

void handleLogin() {
  String password = server.arg("password");
  if (password == WEB_PASSWORD) {
    server.sendHeader("Set-Cookie", "ESP32_AUTH=1; Max-Age=86400; Path=/");
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }
  server.send(403, "text/html; charset=utf-8", "<meta charset='utf-8'><p>密码错误</p><p><a href='/'>返回</a></p>");
}

void handleLogout() {
  server.sendHeader("Set-Cookie", "ESP32_AUTH=0; Max-Age=0; Path=/");
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void handleSend() {
  if (!isAuthenticated()) {
    sendLoginPage();
    return;
  }
  String text = server.arg("text");
  if (!text.length()) {
    server.send(400, "text/plain; charset=utf-8", "Missing text");
    return;
  }
  charDelayMs = readIntArg("charDelay", 70, 1, 800);
  riskCharDelayMs = readIntArg("riskDelay", 140, 1, 1000);
  lineDelayMs = readIntArg("lineDelay", 400, 0, 2000);
  lineStartDelayMs = readIntArg("lineStartDelay", 150, 0, 1000);
  chunkSize = readIntArg("chunkSize", 24, 1, 240);
  chunkDelayMs = readIntArg("chunkDelay", 120, 0, 1000);
  beforeTypeDelayMs = readIntArg("startDelay", 5000, 1000, 30000);
  saveAppSettings();

  String response = "<meta charset='utf-8'><p>已接收，将在 ";
  response += String(beforeTypeDelayMs / 1000.0, 1);
  response += " 秒后开始输入。请把光标放到目标窗口。</p><p><a href='/'>返回</a></p>";
  server.send(200, "text/html; charset=utf-8", response);

  delayWithButtonCheck(beforeTypeDelayMs);
  typingActive = true;
  typeText(text);
  typingActive = false;
  lastKeepAliveAt = millis();
}

void handleSettings() {
  if (!isAuthenticated()) {
    sendLoginPage();
    return;
  }
  keepAliveEnabled = server.hasArg("keepAliveEnabled");
  int intervalSeconds = readIntArg("keepAliveInterval", 180, 10, 86400);
  keepAliveIntervalMs = (unsigned long)intervalSeconds * 1000UL;
  String action = server.arg("keepAliveAction");
  if (isValidKeepAliveAction(action)) keepAliveAction = action;
  else keepAliveAction = DEFAULT_KEEP_ALIVE_ACTION;
  saveAppSettings();
  lastKeepAliveAt = millis();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void handleApiMessage() {
  if (!server.hasArg("plain")) {
    sendHttpJson(makeJsonError("empty_body"));
    return;
  }
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    sendHttpJson(makeJsonError("json_parse_failed"));
    return;
  }
  String token = doc["token"] | "";
  if (!isAuthenticated() && token != WEB_PASSWORD) {
    sendHttpJson(makeJsonError("unauthorized"));
    return;
  }
  String result = handleJsonCommand(doc, true);
  sendHttpJson(result);
  String type = doc["type"] | "";
  if (type == "reboot") {
    delay(300);
    ESP.restart();
  }
}

void handlePortalScan() {
  String json = handleWifiScanCommand();
  JsonDocument doc;
  deserializeJson(doc, json);
  String html = "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>扫描 Wi-Fi</title></head><body><h2>扫描结果</h2>";
  JsonArray arr = doc["networks"].as<JsonArray>();
  for (JsonObject item : arr) {
    String ssid = item["ssid"] | "";
    int rssi = item["rssi"] | 0;
    html += "<p>" + ssid + " ｜ RSSI " + String(rssi) + "</p>";
  }
  html += "<p><a href='/'>返回</a></p></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handlePortalSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (!ssid.length()) {
    server.send(400, "text/html; charset=utf-8", "<meta charset='utf-8'><p>SSID 不能为空</p><p><a href='/'>返回</a></p>");
    return;
  }
  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["password"] = password;
  String result = handleWifiSaveCommand(doc);
  server.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><p>已尝试连接 Wi-Fi。</p><pre>" + result + "</pre><p><a href='/'>返回</a></p>");
}

void handlePortalClear() {
  handleWifiClearCommand();
  server.send(200, "text/html; charset=utf-8", "<meta charset='utf-8'><p>Wi-Fi 配置已清空。</p><p><a href='/'>返回</a></p>");
}

// ===================== setup / loop =====================
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("ESP32-S3 HID Console Firmware start");
  Serial.print("Firmware: ");
  Serial.println(FIRMWARE_VERSION);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  loadAppSettings();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  } else {
    Serial.println("LittleFS mounted");
  }

  connectSavedWifi();

  const char* headerKeys[] = {"Cookie"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/api/message", HTTP_OPTIONS, sendHttpOptions);
  server.on("/api/message", HTTP_POST, handleApiMessage);
  server.on("/portal_scan", HTTP_GET, handlePortalScan);
  server.on("/portal_save", HTTP_POST, handlePortalSave);
  server.on("/portal_clear", HTTP_GET, handlePortalClear);

  server.begin();
  lastKeepAliveAt = millis();
  Serial.println("Web server started.");
}

void loop() {
  handleResetButton();
  server.handleClient();
  handleSerialInput();

  if (keepAliveEnabled && !typingActive) {
    unsigned long now = millis();
    if (now - lastKeepAliveAt >= keepAliveIntervalMs) {
      sendKeepAlive();
      lastKeepAliveAt = now;
    }
  }
}
