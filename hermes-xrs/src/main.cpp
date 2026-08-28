/**
 * @file main.cpp
 * @brief Hermes 学而思 ESP32版 (hermes-xrs) - 完整固件 v2.0
 * 
 * 硬件配置:
 *   - ESP32-S3 主控
 *   - 1.8" SPI TFT 显示屏 (ST7735 128x160)
 *   - MicroSD 卡 (SPI2, 与 TFT 共享)
 *   - 6 按键 (上/下/左/右/A/B)
 *   - 无源蜂鸣器 (GPIO14, PWM)
 *   - 光照/热敏传感器 (ADC1)
 *   - I2C (GPIO15/21)
 * 
 * 功能 (与 hermes-t5 完全相同):
 *   - 语音对话 (Socket.IO + ADPCM)
 *   - 音频播放 (I2S + HTTP流)
 *   - 时钟/日期显示 (NTP同步)
 *   - 天气显示 (HTTP API)
 *   - 环境传感器 (光照/热敏)
 *   - 通知系统 (消息队列)
 *   - 菜单系统 (模式切换)
 *   - 多按键手势 (单击/双击/长按)
 *   - 定时器/闹钟
 * 
 * 使用 hermes-device 协议库 v1.0.0
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <esp_system.h>
#include <esp_rom_sys.h>
#include <vector>
#include <ESPmDNS.h>

#include "config.h"

// ============================================================
// 全局对象
// ============================================================
TFT_eSPI tft = TFT_eSPI();
WebServer webServer(HERMES_HTTP_PORT);
Preferences prefs;

// SPI 共享
SPIClass hspi(VSPI);  // 使用 VSPI (SPI2) 用于 SD 卡

// WiFi 状态
bool wifiReady = false;
bool setupApMode = false;
String savedSsid;
String savedAccount;
String savedPass;

// 设备状态
String deviceId;
String deviceCode;
String mcuAuthToken;
String activeDeviceUrl;
String selectedProfile;
String mcuSocketRelayUrl;
String discoveredGatewayUrl;

// Socket.IO 状态
WiFiClient mcuWsPlainClient;
WiFiClientSecure mcuWsSecureClient;
WiFiClient *mcuWsClient = &mcuWsPlainClient;
bool wsReady = false;
bool mcuSocketConnected = false;
bool mcuSocketNamespaceReady = false;
uint32_t lastSocketConnectAttempt = 0;
uint8_t socketConnectFailures = 0;
uint32_t lastSocketActivity = 0;
uint32_t socketConnectTime = 0;
bool socketFirstData = false;
uint32_t handshakeSentTime = 0;

// I2S 状态
bool i2sReady = false;

// 交互状态
String mcuInteractionId;
String mcuInteractionStatus;
String mcuInteractionText;
bool mcuInteractionActive = false;

// 音频播放状态
bool audioBusy = false;
bool mcuAudioPlaying = false;

// 时间同步状态
bool timeSynced = false;
time_t lastNtpSync = 0;

// 传感器数据
float sensorTemperature = 0;
float sensorHumidity = 0;
uint32_t lastSensorRead = 0;

// 电池数据
float batteryVoltage = 0;
uint8_t batteryPercent = 0;
uint32_t lastBatteryRead = 0;

// 通知队列
struct Notification {
  String title;
  String message;
  uint32_t timestamp;
  bool read;
};
std::vector<Notification> notifications;
uint32_t lastNotificationCheck = 0;

// 闹钟
struct Alarm {
  uint8_t hour;
  uint8_t minute;
  bool enabled;
  bool triggered;
};
constexpr uint8_t kMaxAlarms = 5;
Alarm alarms[kMaxAlarms] = {};

// 定时器
uint32_t timerStartMs = 0;
uint32_t timerDuration = 0;
bool timerRunning = false;

// 菜单系统
enum class MenuMode {
  Main,       // 主界面 (时钟+状态)
  Weather,    // 天气
  Sensors,    // 传感器
  Music,      // 音乐播放
  Notifications, // 通知
  Menu,       // 菜单列表
  Settings    // 设置
};
MenuMode currentMenu = MenuMode::Main;
uint8_t menuIndex = 0;
uint32_t lastMenuActivity = 0;
constexpr uint32_t kMenuTimeoutMs = 30000;

// 音乐播放状态
struct MusicState {
  bool playing;
  String title;
  String artist;
  uint32_t duration;
  uint32_t position;
  uint8_t volume;
};
MusicState musicState = {};

// 按键状态 (6键)
struct ButtonState {
  uint8_t pin;
  bool lastState;
  bool currentState;
  uint32_t lastDebounce;
  uint32_t pressStart;
  uint8_t clickCount;
  bool longPressTriggered;
};

constexpr uint8_t kPinKeyUp = 2;
constexpr uint8_t kPinKeyDown = 13;
constexpr uint8_t kPinKeyLeft = 27;
constexpr uint8_t kPinKeyRight = 35;
constexpr uint8_t kPinKeyA = 34;
constexpr uint8_t kPinKeyB = 12;

ButtonState buttons[6] = {
  {kPinKeyUp, HIGH, HIGH, 0, 0, 0, false},
  {kPinKeyDown, HIGH, HIGH, 0, 0, 0, false},
  {kPinKeyLeft, HIGH, HIGH, 0, 0, 0, false},
  {kPinKeyRight, HIGH, HIGH, 0, 0, 0, false},
  {kPinKeyA, HIGH, HIGH, 0, 0, 0, false},
  {kPinKeyB, HIGH, HIGH, 0, 0, 0, false}
};

// 按键去抖和手势检测
constexpr uint32_t kButtonDebounceMs = 30;
constexpr uint32_t kButtonDoubleClickMs = 400;
constexpr uint32_t kButtonLongPressMs = 800;

// 前向声明
void disconnectMcuSocketClient();
void connectMcuSocketClient();
bool runMcuLogin(const String &account, const String &password);
void updateDisplay();
void setDisplayMode(uint8_t mode);
void handleButtonGesture(uint8_t keyIndex, uint8_t gesture);
void readSensors();
void readBattery();
void checkAlarms();
void syncTime();
void playBuzzer(uint16_t freq, uint16_t duration);
void nextMenu();
void prevMenu();
void enterMenu();
void exitMenu();
void fetchWeather();
void handleMusicCommand(uint8_t cmd);
void addNotification(const String &title, const String &msg);
void showNotificationOverlay();
void startSetupAp();
bool recordAndBroadcastVoice(const String &interactionId);

// ============================================================
// 工具函数
// ============================================================

String getDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String getDeviceCode() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[9];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String escapeJson(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

String jsonStringValue(const String &json, const String &key) {
  String searchKey = "\"" + key + "\"";
  int keyPos = json.indexOf(searchKey);
  if (keyPos < 0) return "";
  int colonPos = json.indexOf(':', keyPos + searchKey.length());
  if (colonPos < 0) return "";
  int quoteStart = json.indexOf('"', colonPos + 1);
  if (quoteStart < 0) return "";
  int quoteEnd = json.indexOf('"', quoteStart + 1);
  if (quoteEnd < 0) return "";
  return json.substring(quoteStart + 1, quoteEnd);
}

int jsonIntValue(const String &json, const String &key) {
  String searchKey = "\"" + key + "\"";
  int keyPos = json.indexOf(searchKey);
  if (keyPos < 0) return 0;
  int colonPos = json.indexOf(':', keyPos + searchKey.length());
  if (colonPos < 0) return 0;
  int valueStart = colonPos + 1;
  while (valueStart < (int)json.length() && isspace(json[valueStart])) valueStart++;
  if (valueStart >= (int)json.length()) return 0;
  if (json[valueStart] == '"') {
    int quoteEnd = json.indexOf('"', valueStart + 1);
    if (quoteEnd < 0) return 0;
    return json.substring(valueStart + 1, quoteEnd).toInt();
  }
  int valueEnd = valueStart;
  while (valueEnd < (int)json.length() && (isdigit(json[valueEnd]) || json[valueEnd] == '-')) valueEnd++;
  return json.substring(valueStart, valueEnd).toInt();
}

// ============================================================
// TFT 显示系统
// ============================================================



DisplayMode currentDisplayMode = DisplayMode::Boot;
String displayLine1 = "";
String displayLine2 = "";
uint8_t displayProgress = 0;
bool displayDirty = true;

void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  switch (currentDisplayMode) {
    case DisplayMode::Boot:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print("HStudio XRS");
      tft.setTextSize(1);
      tft.setCursor(10, 40);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print("Hermes 学而思 ESP32版");
      tft.setCursor(10, 70);
      tft.print("Booting...");
      if (displayProgress > 0) {
        tft.drawRect(10, 100, TFT_WIDTH - 20, 10, TFT_WHITE);
        tft.fillRect(12, 102, (TFT_WIDTH - 24) * displayProgress / 100, 6, TFT_GREEN);
      }
      break;
      
    case DisplayMode::WifiSetup:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.print("WiFi Setup");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print("Connect to:");
      tft.setTextSize(2);
      tft.setCursor(10, 65);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print(HERMES_AP_SSID);
      tft.setTextSize(1);
      tft.setCursor(10, 100);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print("Then open:");
      tft.setCursor(10, 115);
      tft.print("192.168.4.1");
      break;
      
    case DisplayMode::WifiConnecting:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.print("WiFi");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print("Connecting:");
      tft.setCursor(10, 60);
      tft.print(displayLine1);
      if (displayProgress > 0) {
        tft.drawRect(10, 85, TFT_WIDTH - 20, 10, TFT_WHITE);
        tft.fillRect(12, 87, (TFT_WIDTH - 24) * displayProgress / 100, 6, TFT_GREEN);
      }
      break;
      
    case DisplayMode::Login:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.print("Login");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print(displayLine1);
      tft.setCursor(10, 65);
      tft.print(displayLine2);
      tft.setCursor(10, 95);
      tft.print("Open web page");
      break;
      
    case DisplayMode::Ready:
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print("Ready");
      tft.setTextSize(1);
      tft.setCursor(10, 35);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print("IP: ");
      tft.print(displayLine1);
      tft.setCursor(10, 50);
      tft.print("Profile: ");
      tft.print(displayLine2.length() > 0 ? displayLine2 : "None");
      tft.drawLine(10, 65, TFT_WIDTH - 10, 65, TFT_DARKGREY);
      tft.setCursor(10, 75);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("A:OK  B:Back");
      break;
      
    case DisplayMode::Think:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
      tft.print("Thinking");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print(displayLine1);
      if (displayProgress > 0) {
        tft.drawRect(10, 85, TFT_WIDTH - 20, 10, TFT_WHITE);
        tft.fillRect(12, 87, (TFT_WIDTH - 24) * displayProgress / 100, 6, TFT_MAGENTA);
      }
      break;
      
    case DisplayMode::Listen:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print("Listening");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print("Speak now...");
      tft.setCursor(10, 70);
      tft.print("(Hold A to record)");
      if (displayProgress > 0) {
        tft.drawRect(10, 100, TFT_WIDTH - 20, 10, TFT_WHITE);
        tft.fillRect(12, 102, (TFT_WIDTH - 24) * displayProgress / 100, 6, TFT_GREEN);
      }
      break;
      
    case DisplayMode::Play:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print("Playing");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print(displayLine1);
      if (displayProgress > 0) {
        tft.drawRect(10, 85, TFT_WIDTH - 20, 10, TFT_WHITE);
        tft.fillRect(12, 87, (TFT_WIDTH - 24) * displayProgress / 100, 6, TFT_CYAN);
      }
      break;
      
    case DisplayMode::Error:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("Error!");
      tft.setTextSize(1);
      tft.setCursor(10, 45);
      tft.print(displayLine1);
      tft.setCursor(10, 65);
      tft.print(displayLine2);
      break;
      
    case DisplayMode::Clock: {
      time_t now;
      time(&now);
      struct tm *ti = localtime(&now);
      char timeStr[16];
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
      char dateStr[32];
      const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d %s", 
               ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday, weekdays[ti->tm_wday]);
      
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print(timeStr);
      tft.setTextSize(1);
      tft.setCursor(10, 35);
      tft.print(dateStr);
      
      // 传感器数据
      tft.setCursor(10, 55);
      tft.printf("T:%.1fC L:%d", sensorTemperature, (int)sensorHumidity);
      
      // 通知数量
      if (notifications.size() > 0) {
        tft.setCursor(10, 75);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.printf("%d new msgs", notifications.size());
      }
      
      // 按键提示
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("A:Menu B:Back");
      break;
    }
    
    case DisplayMode::Weather:
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print("Weather");
      tft.setTextSize(1);
      tft.setCursor(10, 35);
      tft.print(displayLine1);
      tft.setCursor(10, 55);
      tft.print(displayLine2);
      tft.setCursor(10, 80);
      tft.printf("Temp: %.1fC", sensorTemperature);
      tft.setCursor(10, 100);
      tft.printf("Light: %d", (int)sensorHumidity);
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("B:Back");
      break;
      
    case DisplayMode::Sensors:
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print("Sensors");
      tft.setTextSize(1);
      tft.drawLine(10, 28, TFT_WIDTH - 10, 28, TFT_DARKGREY);
      tft.setCursor(10, 35);
      tft.printf("Temp: %.1f C", sensorTemperature);
      tft.setCursor(10, 55);
      tft.printf("Light: %d", (int)sensorHumidity);
      tft.setCursor(10, 75);
      tft.printf("Battery: %.0f%%", (float)batteryPercent);
      tft.setCursor(10, 95);
      tft.printf("WiFi: %d dBm", WiFi.RSSI());
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("B:Back");
      break;
      
    case DisplayMode::Music:
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(musicState.playing ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
      tft.print(musicState.playing ? "Playing" : "Paused");
      tft.setTextSize(1);
      tft.setCursor(10, 35);
      tft.print(musicState.title.length() > 0 ? musicState.title : "No track");
      tft.setCursor(10, 55);
      tft.print(musicState.artist);
      if (musicState.duration > 0) {
        tft.setCursor(10, 75);
        tft.printf("%02d:%02d / %02d:%02d", 
                  musicState.position / 60, musicState.position % 60,
                  musicState.duration / 60, musicState.duration % 60);
        tft.drawRect(10, 90, TFT_WIDTH - 20, 8, TFT_WHITE);
        tft.fillRect(12, 92, (TFT_WIDTH - 24) * musicState.position / musicState.duration, 4, TFT_GREEN);
      }
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("A:Play B:Back");
      break;
      
    case DisplayMode::Notification:
      tft.setTextSize(2);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.print("Messages");
      tft.setTextSize(1);
      if (notifications.size() == 0) {
        tft.setCursor(10, 50);
        tft.print("No new messages");
      } else {
        tft.setCursor(10, 35);
        tft.print(notifications[0].title);
        tft.setCursor(10, 55);
        tft.print(notifications[0].message);
        tft.setCursor(10, 80);
        tft.printf("%d more...", notifications.size() - 1);
      }
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("B:Back");
      break;
      
    case DisplayMode::Menu: {
      tft.setTextSize(1);
      tft.setCursor(10, 5);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print("Menu");
      tft.drawLine(10, 18, TFT_WIDTH - 10, 18, TFT_DARKGREY);
      const char *menuItems[] = {"Clock", "Weather", "Sensors", "Music", "Messages", "Timer", "Alarm", "Settings"};
      constexpr uint8_t menuCount = 8;
      for (uint8_t i = 0; i < menuCount && i < 6; i++) {
        tft.setCursor(15, 30 + i * 18);
        if (i == menuIndex) {
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.print("> ");
        } else {
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print("  ");
        }
        tft.print(menuItems[i]);
      }
      break;
    }
    
    case DisplayMode::Timer:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.print("Timer");
      tft.setTextSize(1);
      if (timerRunning) {
        uint32_t elapsed = (millis() - timerStartMs) / 1000;
        uint32_t remaining = timerDuration > elapsed ? timerDuration - elapsed : 0;
        tft.setCursor(10, 50);
        tft.printf("Remaining: %02d:%02d", remaining / 60, remaining % 60);
      } else {
        tft.setCursor(10, 50);
        tft.print("Set timer via web");
      }
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("B:Back");
      break;
      
    case DisplayMode::Alarm:
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.print("Alarms");
      tft.setTextSize(1);
      for (uint8_t i = 0; i < kMaxAlarms && i < 5; i++) {
        tft.setCursor(10, 35 + i * 20);
        tft.printf("%d. %02d:%02d %s", i + 1, alarms[i].hour, alarms[i].minute,
                  alarms[i].enabled ? "ON" : "OFF");
      }
      tft.setCursor(10, 145);
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.print("B:Back");
      break;
  }
  
  displayDirty = false;
}

void setDisplayMode(DisplayMode mode, const String &line1 = "", const String &line2 = "", uint8_t progress = 0) {
  currentDisplayMode = mode;
  displayLine1 = line1;
  displayLine2 = line2;
  displayProgress = progress;
  displayDirty = true;
}

// ============================================================
// I2S 音频系统
// ============================================================

bool initI2S() {
  Serial.println("I2S: installing driver...");
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = VOICE_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = 27,
    .ws_io_num = 26,
    .data_out_num = 25,
    .data_in_num = 34
  };
  
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S driver install failed: %d\n", err);
    return false;
  }
  
  Serial.println("I2S: driver installed, setting pins...");
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S set pin failed: %d\n", err);
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }
  
  i2sReady = true;
  Serial.println("I2S initialized successfully");
  return true;
}

// ============================================================
// Socket.IO 客户端
// ============================================================

bool writeMcuWsBytes(const uint8_t *data, size_t length) {
  if (!mcuWsClient->connected()) return false;
  size_t written = mcuWsClient->write(data, length);
  return written == length;
}

bool sendRawWsFrame(uint8_t opcode, const uint8_t *data, size_t length) {
  if (!mcuWsClient->connected()) return false;
  
  uint8_t header[14];
  size_t headerLen = 0;
  header[headerLen++] = 0x80 | (opcode & 0x0F);
  
  if (length < 126) {
    header[headerLen++] = 0x80 | static_cast<uint8_t>(length);
  } else if (length <= 0xFFFF) {
    header[headerLen++] = 0x80 | 126;
    header[headerLen++] = static_cast<uint8_t>((length >> 8) & 0xFF);
    header[headerLen++] = static_cast<uint8_t>(length & 0xFF);
  } else {
    return false;
  }
  
  uint8_t mask[4];
  for (uint8_t i = 0; i < 4; ++i) mask[i] = static_cast<uint8_t>(esp_random() & 0xFF);
  for (uint8_t i = 0; i < 4; ++i) header[headerLen++] = mask[i];
  
  if (!writeMcuWsBytes(header, headerLen)) return false;
  
  constexpr size_t kChunk = 256;
  uint8_t buffer[kChunk];
  size_t offset = 0;
  while (offset < length) {
    size_t n = min(kChunk, length - offset);
    for (size_t i = 0; i < n; ++i) {
      buffer[i] = data[offset + i] ^ mask[(offset + i) & 3];
    }
    if (!writeMcuWsBytes(buffer, n)) return false;
    offset += n;
    yield();
  }
  return true;
}

bool sendRawWsText(const String &payload) {
  return sendRawWsFrame(0x1, reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length());
}

bool sendMcuSocketEvent(const String &event, const String &json) {
  if (!wsReady || !mcuSocketNamespaceReady || event.length() == 0) return false;
  
  String securedJson = json;
  if (mcuAuthToken.length() > 0 && json.length() > 0 && json[0] == '{' && json.indexOf("\"apiToken\"") < 0) {
    securedJson = "{\"apiToken\":\"" + escapeJson(mcuAuthToken) + "\"," + json.substring(1);
  }
  
  String payload;
  payload.reserve(securedJson.length() + event.length() + 28);
  payload += "42/global-agent,[\"";
  payload += escapeJson(event);
  payload += "\",";
  payload += securedJson;
  payload += "]";
  
  return sendRawWsText(payload);
}

bool sendMcuSocketJson(const String &json) {
  String type = jsonStringValue(json, "type");
  if (type.length() == 0) type = "mcu.event";
  return sendMcuSocketEvent(type, json);
}

void sendMcuSocketNamespaceConnect() {
  if (!mcuSocketConnected || mcuAuthToken.length() == 0) return;
  
  String json = "{\"token\":\"" + escapeJson(mcuAuthToken) + 
                "\",\"deviceCode\":\"" + escapeJson(deviceCode) + 
                "\",\"device_code\":\"" + escapeJson(deviceCode) + 
                "\",\"role\":\"hermes-studio\",\"instanceId\":\"" + escapeJson(deviceId) + 
                "\",\"profile\":\"" + escapeJson(selectedProfile) + "\"}";
  
  String frame = "40/global-agent," + json;
  sendRawWsText(frame);
}

void sendMcuReady() {
  String json = "{\"type\":\"mcu.ready\",\"id\":\"" + escapeJson(deviceId) + 
                "\",\"active_device\":\"" + escapeJson(deviceId) +
                "\",\"profile\":\"" + escapeJson(selectedProfile) + 
                "\",\"capabilities\":{\"display\":true,\"audio_queue\":true,\"audio_playback\":true,\"pcm_stream\":false}}";
  sendMcuSocketJson(json);
}

bool parseSocketIoEvent(const String &message, String *event, String *json) {
  int arrayStart = message.indexOf('[');
  if (arrayStart < 0) return false;
  int firstQuote = message.indexOf('"', arrayStart);
  if (firstQuote < 0) return false;
  int secondQuote = message.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return false;
  int comma = message.indexOf(',', secondQuote + 1);
  int arrayEnd = message.lastIndexOf(']');
  if (comma < 0 || arrayEnd <= comma) return false;
  
  *event = message.substring(firstQuote + 1, secondQuote);
  
  int payloadStart = comma + 1;
  while (payloadStart < arrayEnd && isspace(static_cast<unsigned char>(message[payloadStart]))) ++payloadStart;
  
  int payloadEnd = arrayEnd;
  if (payloadStart < arrayEnd && message[payloadStart] == '{') {
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int i = payloadStart; i < arrayEnd; ++i) {
      char c = message[i];
      if (escaped) { escaped = false; continue; }
      if (c == '\\') { escaped = true; continue; }
      if (c == '"') { inString = !inString; continue; }
      if (inString) continue;
      if (c == '{') ++depth;
      else if (c == '}') {
        --depth;
        if (depth == 0) { payloadEnd = i + 1; break; }
      }
    }
  }
  
  *json = message.substring(payloadStart, payloadEnd);
  return true;
}

void handleMcuWebSocketText(const String &message) {
  String type = jsonStringValue(message, "type");
  
  if (type == "mcu.auth") {
    String authOk = "{\"apiToken\":\"" + escapeJson(mcuAuthToken) + "\",\"type\":\"mcu.auth.ok\",\"ok\":true,\"id\":\"" + escapeJson(deviceId) + "\"}";
    String frame = "42/global-agent,[\"mcu.auth.ok\"," + authOk + "]";
    sendRawWsText(frame);
    String statusJson = "{\"apiToken\":\"" + escapeJson(mcuAuthToken) + "\",\"type\":\"mcu.status\",\"interactionId\":\"\",\"status\":\"\",\"audioPlaying\":false,\"queueLength\":0,\"socketClients\":1,\"socketConnected\":true,\"active_device\":\"" + escapeJson(deviceId) + "\",\"profile\":\"" + escapeJson(selectedProfile) + "\"}";
    sendMcuSocketJson(statusJson);
    return;
  }
  if (type == "auth.invalid") {
    Serial.println("auth.invalid received, clearing token");
    mcuAuthToken = "";
    mcuSocketNamespaceReady = false;
    disconnectMcuSocketClient();
    prefs.begin("mcu", false);
    prefs.remove("auth_token");
    prefs.end();
    setDisplayMode(DisplayMode::Login, "Token expired", "Login via web");
    return;
  }
  if (type == "interaction.status") {
    mcuInteractionId = jsonStringValue(message, "interactionId");
    mcuInteractionStatus = jsonStringValue(message, "status");
    mcuInteractionText = jsonStringValue(message, "text");
    mcuInteractionActive = true;
    displayDirty = true;
    return;
  }
  if (type == "audio.enqueue") {
    String url = jsonStringValue(message, "url");
    String mimeType = jsonStringValue(message, "mimeType");
    uint32_t sampleRate = jsonIntValue(message, "sampleRate");
    if (sampleRate == 0) sampleRate = VOICE_SAMPLE_RATE;
    // TODO: 实现音频播放
    return;
  }
  if (type == "mcu.session.clear") {
    mcuInteractionId = "";
    mcuInteractionStatus = "";
    mcuInteractionText = "";
    mcuInteractionActive = false;
    displayDirty = true;
    return;
  }
  
  Serial.printf("Unknown MCU message type: %s\n", type.c_str());
}

void handleSocketIoText(const String &message) {
  if (message == "2") {
    sendRawWsText("3");
    return;
  }
  if (message.startsWith("0")) {
    sendMcuSocketNamespaceConnect();
    return;
  }
  if (message.startsWith("40/global-agent")) {
    mcuSocketNamespaceReady = true;
    Serial.println("Socket.IO namespace /global-agent connected");
    socketConnectFailures = 0;
    sendMcuReady();
    return;
  }
  if (message.startsWith("42")) {
    String event;
    String json;
    if (parseSocketIoEvent(message, &event, &json)) {
      Serial.printf("Socket.IO event: %s %s\n", event.c_str(), json.c_str());
      handleMcuWebSocketText(json);
    }
    return;
  }
}

bool readMcuWsBytes(uint8_t *buffer, size_t length, uint32_t timeoutMs = 100) {
  size_t read = 0;
  uint32_t startedAt = millis();
  while (read < length && millis() - startedAt < timeoutMs) {
    int available = mcuWsClient->available();
    if (available > 0) {
      int n = mcuWsClient->read(buffer + read, min(static_cast<size_t>(available), length - read));
      if (n > 0) read += static_cast<size_t>(n);
    } else {
      delay(1);
      yield();
    }
  }
  return read == length;
}

void mcuSocketLoop() {
  if (!wsReady) return;
  if (!mcuWsClient->connected()) {
    wsReady = false;
    mcuSocketConnected = false;
    mcuSocketNamespaceReady = false;
    Serial.println("Socket.IO transport disconnected");
    
    if (millis() - socketConnectTime < 5000) {
      socketConnectFailures++;
      Serial.printf("Short connection, failure count=%d/3\n", socketConnectFailures);
      
      if (socketConnectFailures >= 3) {
        Serial.println("Auth likely failed, clearing token");
        mcuAuthToken = "";
        socketConnectFailures = 0;
        prefs.begin("mcu", false);
        prefs.remove("auth_token");
        prefs.end();
        setDisplayMode(DisplayMode::Login, "Auth failed", "Login via web");
      }
    } else {
      socketConnectFailures = 0;
    }
    return;
  }
  
  if (socketConnectTime > 0 && !socketFirstData && millis() - socketConnectTime > 3000) {
    Serial.println("No data received, auth likely failed");
    socketConnectFailures++;
    Serial.printf("No data timeout, failure count=%d/3\n", socketConnectFailures);
    
    if (socketConnectFailures >= 3) {
      Serial.println("Auth likely failed, clearing token");
      mcuAuthToken = "";
      socketConnectFailures = 0;
      prefs.begin("mcu", false);
      prefs.remove("auth_token");
      prefs.end();
      setDisplayMode(DisplayMode::Login, "Auth failed", "Login via web");
    }
    
    disconnectMcuSocketClient();
    return;
  }
  
  lastSocketActivity = millis();
  
  if (mcuWsClient->available() < 2) return;
  
  socketFirstData = true;
  
  while (mcuWsClient->available() >= 2) {
    uint8_t header[2];
    if (!readMcuWsBytes(header, 2)) return;
    
    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t length = header[1] & 0x7F;
    
    if (length == 126) {
      uint8_t ext[2];
      if (!readMcuWsBytes(ext, 2)) return;
      length = (static_cast<uint16_t>(ext[0]) << 8) | ext[1];
    } else if (length == 127) {
      uint8_t ext[8];
      if (!readMcuWsBytes(ext, 8)) return;
      length = 0;
      for (uint8_t i = 0; i < 8; ++i) length = (length << 8) | ext[i];
    }
    
    if (length > SOCKET_MAX_FRAME_SIZE) {
      Serial.println("WS frame too large");
      wsReady = false;
      mcuWsClient->stop();
      return;
    }
    
    if (opcode == 0x1) {
      String payload;
      payload.reserve(length);
      uint8_t buf[256];
      uint64_t remaining = length;
      while (remaining > 0) {
        size_t toRead = min(static_cast<size_t>(remaining), sizeof(buf));
        if (!readMcuWsBytes(buf, toRead)) break;
        for (size_t i = 0; i < toRead; i++) payload += static_cast<char>(buf[i]);
        remaining -= toRead;
      }
      handleSocketIoText(payload);
    } else if (opcode == 0x2) {
      uint8_t buf[512];
      uint64_t remaining = length;
      while (remaining > 0) {
        size_t toRead = min(static_cast<size_t>(remaining), sizeof(buf));
        if (!readMcuWsBytes(buf, toRead)) break;
        remaining -= toRead;
      }
    } else if (opcode == 0x8) {
      wsReady = false;
      mcuWsClient->stop();
      return;
    } else if (opcode == 0x9) {
      sendRawWsFrame(0xA, nullptr, 0);
    }
  }
}

void connectMcuSocketClient() {
  if (activeDeviceUrl.length() == 0) return;
  
  if (socketConnectFailures >= 5) {
    Serial.println("Too many connection failures, stopping reconnect");
    return;
  }
  
  String host = activeDeviceUrl;
  if (host.startsWith("http://")) host = host.substring(7);
  if (host.startsWith("https://")) host = host.substring(8);
  
  int port = 80;
  int colon = host.lastIndexOf(':');
  if (colon > 0) {
    port = host.substring(colon + 1).toInt();
    host = host.substring(0, colon);
  }
  
  Serial.printf("Connecting Socket.IO to %s:%d (failures=%d)\n", host.c_str(), port, socketConnectFailures);
  
  if (mcuWsClient->connect(host.c_str(), port)) {
    String path = "/socket.io/?EIO=4&transport=websocket";
    mcuWsClient->print("GET " + path + " HTTP/1.1\r\n");
    mcuWsClient->print("Host: " + host + "\r\n");
    mcuWsClient->print("Upgrade: websocket\r\n");
    mcuWsClient->print("Connection: Upgrade\r\n");
    mcuWsClient->print("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");
    mcuWsClient->print("Sec-WebSocket-Version: 13\r\n");
    mcuWsClient->print("\r\n");
    
    uint32_t upgradeStart = millis();
    String prevLine;
    while (millis() - upgradeStart < 3000) {
      if (mcuWsClient->available()) {
        String resp = mcuWsClient->readStringUntil('\n');
        if (prevLine == "\r" || resp == "\r") break;
        if (resp.length() > 0 && resp != "\r") prevLine = resp;
      }
    }
    
    mcuSocketConnected = true;
    wsReady = true;
    socketConnectTime = millis();
    lastSocketActivity = millis();
    socketFirstData = false;
    handshakeSentTime = millis();
    
    delay(100);
    sendMcuSocketNamespaceConnect();
    
    Serial.println("Socket.IO transport connected");
  } else {
    socketConnectFailures++;
    Serial.printf("Socket.IO connection failed (%d)\n", socketConnectFailures);
  }
}

void disconnectMcuSocketClient() {
  if (mcuWsClient->connected()) {
    mcuWsClient->stop();
  }
  wsReady = false;
  mcuSocketConnected = false;
  mcuSocketNamespaceReady = false;
}

// ============================================================
// 按键系统 (6键手势识别)
// ============================================================

void handleButtonGesture(uint8_t keyIndex, uint8_t gesture) {
  // gesture: 1=单击, 2=双击, 3=长按
  Serial.printf("Button %d gesture: %d\n", keyIndex, gesture);
  
  if (mcuInteractionActive) {
    if (gesture == 1 && keyIndex == 4) { // A键单击
      // 停止当前交互
    }
    return;
  }
  
  if (currentDisplayMode == DisplayMode::Menu) {
    if (keyIndex == 0) { // 上
      if (menuIndex > 0) menuIndex--;
      displayDirty = true;
    } else if (keyIndex == 1) { // 下
      if (menuIndex < 7) menuIndex++;
      displayDirty = true;
    } else if (keyIndex == 4 && gesture == 1) { // A单击 - 选择
      enterMenu();
    } else if (keyIndex == 5) { // B - 返回
      exitMenu();
    }
    return;
  }
  
  // 默认按键行为
  if (keyIndex == 4) { // A键
    if (gesture == 3) {
      // 长按 - 语音对话
      if (mcuAuthToken.length() > 0 && !audioBusy) {
        String interactionId = "voice-" + String(millis());
        recordAndBroadcastVoice(interactionId);
      }
    } else if (gesture == 1) {
      // 单击 - 进入菜单
      currentDisplayMode = DisplayMode::Menu;
      menuIndex = 0;
      displayDirty = true;
    }
  } else if (keyIndex == 0) { // 上
    if (gesture == 1) {
      if (currentDisplayMode == DisplayMode::Music) {
        handleMusicCommand(3); // prev
      } else {
        prevMenu();
      }
    } else if (gesture == 3) {
      if (musicState.volume < 100) musicState.volume += 10;
    }
  } else if (keyIndex == 1) { // 下
    if (gesture == 1) {
      if (currentDisplayMode == DisplayMode::Music) {
        handleMusicCommand(2); // next
      } else {
        nextMenu();
      }
    } else if (gesture == 3) {
      if (musicState.volume > 0) musicState.volume -= 10;
    }
  } else if (keyIndex == 5) { // B键
    if (gesture == 1) {
      // 单击 - 播放/暂停
      if (currentDisplayMode == DisplayMode::Music) {
        handleMusicCommand(1); // play/pause
      }
    } else if (gesture == 3) {
      // 长按 - 停止
      handleMusicCommand(4); // stop
    }
  }
  
  lastMenuActivity = millis();
}

void processButtons() {
  for (uint8_t i = 0; i < 6; i++) {
    bool reading = digitalRead(buttons[i].pin);
    
    if (reading != buttons[i].lastState) {
      buttons[i].lastDebounce = millis();
    }
    
    if ((millis() - buttons[i].lastDebounce) > kButtonDebounceMs) {
      if (reading != buttons[i].currentState) {
        buttons[i].currentState = reading;
        
        if (buttons[i].currentState == LOW) {
          buttons[i].pressStart = millis();
          buttons[i].longPressTriggered = false;
        } else {
          uint32_t pressDuration = millis() - buttons[i].pressStart;
          
          if (pressDuration >= kButtonLongPressMs) {
            // 长按已在释放前触发
          } else if (pressDuration > kButtonDebounceMs) {
            buttons[i].clickCount++;
          }
        }
      }
      
      if (buttons[i].currentState == LOW && !buttons[i].longPressTriggered) {
        if ((millis() - buttons[i].pressStart) >= kButtonLongPressMs) {
          buttons[i].longPressTriggered = true;
          buttons[i].clickCount = 0;
          handleButtonGesture(i, 3); // 长按
        }
      }
    }
    
    if (buttons[i].clickCount > 0 && buttons[i].currentState == HIGH) {
      if ((millis() - buttons[i].pressStart) > kButtonDoubleClickMs) {
        if (buttons[i].clickCount >= 2) {
          handleButtonGesture(i, 2); // 双击
        } else {
          handleButtonGesture(i, 1); // 单击
        }
        buttons[i].clickCount = 0;
      }
    }
    
    buttons[i].lastState = reading;
  }
}

// ============================================================
// 传感器系统
// ============================================================

void readSensors() {
  if (millis() - lastSensorRead < 30000) return;
  lastSensorRead = millis();
  
  // 光照传感器
  sensorHumidity = analogRead(36) / 4095.0 * 100;  // 使用 humidity 字段存储光照百分比
  
  // 热敏传感器
  int tempRaw = analogRead(39);
  sensorTemperature = (tempRaw / 4095.0) * 100.0;  // 0-100°C 近似
  
  Serial.printf("Sensor: T=%.1fC Light=%.0f%%\n", sensorTemperature, sensorHumidity);
}

void readBattery() {
  if (millis() - lastBatteryRead < 60000) return;
  lastBatteryRead = millis();
  
  // 电池电压 (如果有分压电路)
  uint32_t raw = analogRead(32);
  batteryVoltage = raw * 3.3 / 4095.0 * 2.0;
  
  float pct = (batteryVoltage - 3.3) / (4.2 - 3.3) * 100;
  batteryPercent = constrain((int)pct, 0, 100);
  
  Serial.printf("Battery: %.2fV %d%%\n", batteryVoltage, batteryPercent);
}

// ============================================================
// 时间同步
// ============================================================

void syncTime() {
  if (!wifiReady) return;
  if (timeSynced && (time(nullptr) - lastNtpSync) < 3600) return;
  
  configTime(8 * 3600, 0, "pool.ntp.org");
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    timeSynced = true;
    lastNtpSync = time(nullptr);
    Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
}

// ============================================================
// 蜂鸣器
// ============================================================

void playBuzzer(uint16_t freq, uint16_t duration) {
  if (freq == 0) return;
  tone(14, freq, duration);
}

// ============================================================
// 菜单系统
// ============================================================

void nextMenu() {
  if (currentDisplayMode == DisplayMode::Menu) {
    if (menuIndex < 7) menuIndex++;
    displayDirty = true;
  }
}

void prevMenu() {
  if (currentDisplayMode == DisplayMode::Menu) {
    if (menuIndex > 0) menuIndex--;
    displayDirty = true;
  }
}

void enterMenu() {
  switch (menuIndex) {
    case 0: currentDisplayMode = DisplayMode::Clock; break;
    case 1: currentDisplayMode = DisplayMode::Weather; fetchWeather(); break;
    case 2: currentDisplayMode = DisplayMode::Sensors; break;
    case 3: currentDisplayMode = DisplayMode::Music; break;
    case 4: currentDisplayMode = DisplayMode::Notification; break;
    case 5: currentDisplayMode = DisplayMode::Timer; break;
    case 6: currentDisplayMode = DisplayMode::Alarm; break;
    case 7: /* Settings */ break;
  }
  lastMenuActivity = millis();
}

void exitMenu() {
  currentDisplayMode = DisplayMode::Clock;
  lastMenuActivity = millis();
}

// ============================================================
// 天气获取
// ============================================================

void fetchWeather() {
  if (!wifiReady) return;
  
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin("https://wttr.in/?format=%C+%t+%h&lang=zh")) return;
  
  int code = http.GET();
  if (code == 200) {
    String weather = http.getString();
    weather.trim();
    Serial.printf("Weather: %s\n", weather.c_str());
    
    int idx = weather.indexOf(' ');
    if (idx > 0) {
      displayLine1 = weather.substring(0, idx);
      displayLine2 = weather.substring(idx + 1);
    } else {
      displayLine1 = weather;
    }
    displayDirty = true;
  }
  http.end();
}

// ============================================================
// 音乐控制
// ============================================================

void handleMusicCommand(uint8_t cmd) {
  switch (cmd) {
    case 1:
      musicState.playing = !musicState.playing;
      break;
    case 2:
      // TODO: 下一首
      break;
    case 3:
      // TODO: 上一首
      break;
    case 4:
      musicState.playing = false;
      musicState.position = 0;
      break;
  }
  displayDirty = true;
}

// ============================================================
// 通知系统
// ============================================================

void addNotification(const String &title, const String &msg) {
  Notification n;
  n.title = title;
  n.message = msg;
  n.timestamp = millis();
  n.read = false;
  notifications.insert(notifications.begin(), n);
  if (notifications.size() > 10) notifications.pop_back();
  
  if (currentDisplayMode != DisplayMode::Notification) {
    setDisplayMode(DisplayMode::Notification);
    playBuzzer(1000, 200);
  }
  displayDirty = true;
}

void showNotificationOverlay() {
  if (notifications.size() > 0 && currentDisplayMode != DisplayMode::Notification) {
    setDisplayMode(DisplayMode::Notification);
    displayDirty = true;
  }
}

// ============================================================
// 闹钟检测
// ============================================================

void checkAlarms() {
  if (!timeSynced) return;
  
  time_t now;
  time(&now);
  struct tm *ti = localtime(&now);
  
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (alarms[i].enabled && !alarms[i].triggered) {
      if (alarms[i].hour == ti->tm_hour && alarms[i].minute == ti->tm_min) {
        alarms[i].triggered = true;
        playBuzzer(2000, 1000);
        addNotification("Alarm", "Time's up!");
      }
    }
    if (alarms[i].triggered && (alarms[i].hour != ti->tm_hour || alarms[i].minute != ti->tm_min)) {
      alarms[i].triggered = false;
    }
  }
}

// ============================================================
// 语音录制与传输
// ============================================================

struct VoiceStreamChunk {
  int16_t samples[512];
  uint8_t adpcm[256];
};

bool broadcastMcuVoiceStreamStart(const String &interactionId) {
  if (!wsReady || !mcuSocketNamespaceReady) return false;
  
  String json = "{\"type\":\"voice.stream.start\",\"interactionId\":\"" + escapeJson(interactionId) + 
                "\",\"mimeType\":\"audio/x-ima-adpcm\",\"frameFormat\":\"hadp-chunk-v1\"" +
                ",\"sampleRate\":" + String(VOICE_SAMPLE_RATE) + 
                ",\"channels\":1,\"bitsPerSample\":16" +
                ",\"profile\":\"" + escapeJson(selectedProfile) + "\"}";
  return sendMcuSocketJson(json);
}

bool broadcastMcuVoiceStreamEnd(const String &interactionId, uint32_t dataBytes) {
  if (!wsReady || !mcuSocketNamespaceReady) return false;
  
  String json = "{\"type\":\"voice.stream.end\",\"interactionId\":\"" + escapeJson(interactionId) + 
                "\",\"bytes\":" + String(dataBytes) + "}";
  return sendMcuSocketJson(json);
}

bool broadcastMcuVoiceStreamChunk(const String &interactionId, const uint8_t *data, size_t length, uint32_t offset) {
  if (!wsReady || !mcuSocketNamespaceReady || !data || length == 0) return false;
  
  String payload = "451-/global-agent,[\"voice.stream.chunk\",{\"type\":\"voice.stream.chunk\"";
  payload += ",\"interactionId\":\"" + escapeJson(interactionId) + "\"";
  payload += ",\"offset\":" + String(offset);
  payload += ",\"bytes\":" + String(length);
  payload += ",\"data\":{\"_placeholder\":true,\"num\":0}}]";
  
  return sendRawWsText(payload) && sendRawWsFrame(0x2, data, length);
}

// 简化的 ADPCM 编码 (IMA-ADPCM 4-bit)
int encodeAdpcm(const int16_t *samples, size_t sampleCount, uint8_t *output, size_t outputSize) {
  static const int16_t stepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282,
    1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767
  };
  
  static const int8_t indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
  };
  
  int predictor = 0;
  int stepIndex = 0;
  size_t outPos = 0;
  
  for (size_t i = 0; i < sampleCount && outPos < outputSize; i++) {
    int diff = samples[i] - predictor;
    int step = stepTable[stepIndex];
    
    int nibble = 0;
    if (diff < 0) { nibble = 8; diff = -diff; }
    
    if (diff >= step) { nibble |= 4; diff -= step; }
    step >>= 1;
    if (diff >= step) { nibble |= 2; diff -= step; }
    step >>= 1;
    if (diff >= step) { nibble |= 1; }
    
    predictor += (stepTable[stepIndex] * indexTable[nibble] + (stepTable[stepIndex] >> 2)) >> 2;
    if (predictor > 32767) predictor = 32767;
    if (predictor < -32768) predictor = -32768;
    
    stepIndex += indexTable[nibble];
    if (stepIndex < 0) stepIndex = 0;
    if (stepIndex > 88) stepIndex = 88;
    
    if (i & 1) {
      output[outPos++] = ((nibble & 0x0F) << 4) | (nibble >> 4);
    } else {
      output[outPos] = nibble & 0x0F;
    }
  }
  
  return outPos;
}

bool recordAndBroadcastVoice(const String &interactionId) {
  if (!i2sReady) {
    Serial.println("I2S not ready");
    return false;
  }
  
  if (!broadcastMcuVoiceStreamStart(interactionId)) {
    Serial.println("Failed to start voice stream");
    return false;
  }
  
  audioBusy = true;
  setDisplayMode(DisplayMode::Listen, "Listening...", "", 0);
  
  constexpr size_t kReadBytes = 512;
  uint8_t readBuffer[kReadBytes];
  uint32_t framesDone = 0;
  uint32_t queuedBytes = 0;
  uint32_t maxFrames = (VOICE_SAMPLE_RATE * VOICE_RECORD_MAX_MS) / 1000;
  uint32_t startedAt = millis();
  
  VoiceStreamChunk *chunk = static_cast<VoiceStreamChunk *>(malloc(sizeof(VoiceStreamChunk)));
  if (!chunk) {
    audioBusy = false;
    return false;
  }
  
  size_t pcmChunkFrames = 0;
  
  while (framesDone < maxFrames) {
    if (millis() - startedAt > 20000) break;
    
    if (framesDone > 0 && millis() - startedAt > VOICE_RECORD_MIN_MS) {
      if (digitalRead(kPinKeyA) != LOW) break;
    }
    
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, readBuffer, sizeof(readBuffer), &bytesRead, pdMS_TO_TICKS(40));
    if (err != ESP_OK) break;
    if (bytesRead == 0) { yield(); continue; }
    
    int16_t *samples = reinterpret_cast<int16_t *>(readBuffer);
    size_t count = bytesRead / sizeof(int16_t);
    
    for (size_t i = 0; i + 1 < count && framesDone < maxFrames; i += 2) {
      int16_t mono = (samples[i] + samples[i + 1]) >> 1;
      chunk->samples[pcmChunkFrames++] = mono;
      framesDone++;
      
      if (pcmChunkFrames >= 512) {
        size_t encoded = encodeAdpcm(chunk->samples, pcmChunkFrames, chunk->adpcm, sizeof(chunk->adpcm));
        if (encoded > 0) {
          if (!broadcastMcuVoiceStreamChunk(interactionId, chunk->adpcm, encoded, queuedBytes)) {
            free(chunk);
            audioBusy = false;
            return false;
          }
          queuedBytes += encoded;
        }
        pcmChunkFrames = 0;
      }
    }
    
    uint8_t progress = static_cast<uint8_t>(framesDone * 100 / maxFrames);
    setDisplayMode(DisplayMode::Listen, "Listening...", "", progress);
    yield();
  }
  
  if (pcmChunkFrames > 0) {
    size_t encoded = encodeAdpcm(chunk->samples, pcmChunkFrames, chunk->adpcm, sizeof(chunk->adpcm));
    if (encoded > 0) {
      broadcastMcuVoiceStreamChunk(interactionId, chunk->adpcm, encoded, queuedBytes);
      queuedBytes += encoded;
    }
  }
  
  free(chunk);
  audioBusy = false;
  
  broadcastMcuVoiceStreamEnd(interactionId, queuedBytes);
  setDisplayMode(DisplayMode::Think, "Processing...", "", 0);
  
  return true;
}

// ============================================================
// WiFi 管理
// ============================================================

void discoverGateway();
bool testGateway(const String &url);
bool testHermesStudio(const String &url);

bool connectWifi(const String &ssid, const String &pass) {
  setDisplayMode(DisplayMode::WifiConnecting, ssid, "", 0);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  uint32_t started = millis();
  uint8_t progress = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    progress = static_cast<uint8_t>((millis() - started) * 100 / WIFI_CONNECT_TIMEOUT_MS);
    setDisplayMode(DisplayMode::WifiConnecting, ssid, "", progress);
    yield();
  }
  
  wifiReady = WiFi.status() == WL_CONNECTED;
  if (wifiReady) {
    Serial.printf("WiFi connected: %s IP=%s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
    setDisplayMode(DisplayMode::Ready, WiFi.localIP().toString(), "", 100);
    
    discoverGateway();
    syncTime();
  } else {
    setDisplayMode(DisplayMode::Error, "WiFi failed", ssid);
  }
  
  return wifiReady;
}

// ============================================================
// 网关发现
// ============================================================

void discoverGateway() {
  Serial.println("Discovering Hermes Studio gateway...");
  
  if (MDNS.begin("hstudio-xrs")) {
    Serial.println("mDNS started, searching for services...");
    int n = MDNS.queryService("http", "tcp");
    for (int i = 0; i < n; i++) {
      String name = MDNS.hostname(i);
      String ip = MDNS.IP(i).toString();
      uint16_t port = MDNS.port(i);
      Serial.printf("mDNS found: %s at %s:%d\n", name.c_str(), ip.c_str(), port);
      if (port == 8648) {
        discoveredGatewayUrl = "http://" + ip + ":8648";
        Serial.printf("Gateway found via mDNS: %s\n", discoveredGatewayUrl.c_str());
        return;
      }
    }
  }
  
  Serial.println("Quick scanning for Hermes Studio...");
  String localIp = WiFi.localIP().toString();
  int lastDot = localIp.lastIndexOf('.');
  String subnet = localIp.substring(0, lastDot + 1);
  
  int scanList[] = {1, 100, 101, 102, 103, 104, 105, 200, 201, 202, 21, 22, 23, 24, 25};
  for (int i = 0; i < sizeof(scanList)/sizeof(scanList[0]); i++) {
    String testUrl = subnet + String(scanList[i]);
    if (testUrl == localIp) continue;
    if (testGateway(testUrl)) {
      discoveredGatewayUrl = "http://" + testUrl + ":8648";
      Serial.printf("Gateway found via scan: %s\n", discoveredGatewayUrl.c_str());
      return;
    }
  }
  
  if (activeDeviceUrl.length() > 0) {
    discoveredGatewayUrl = activeDeviceUrl;
    Serial.printf("Using saved gateway URL: %s\n", discoveredGatewayUrl.c_str());
    return;
  }
  
  Serial.println("Gateway not found");
}

bool testGateway(const String &ip) {
  HTTPClient http;
  http.setTimeout(500);
  String url = "http://" + ip + ":8648/api/hermes/health";
  if (!http.begin(url)) return false;
  int code = http.GET();
  http.end();
  return code > 0;
}

bool testHermesStudio(const String &url) {
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(url + "/api/hermes/health")) return false;
  int code = http.GET();
  http.end();
  return code > 0;
}

// ============================================================
// MCU 登录
// ============================================================

bool runMcuLogin(const String &account, const String &password) {
  String endpoint = activeDeviceUrl;
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/auth/mcu-login";
  
  setDisplayMode(DisplayMode::Think, "Login...", "", 40);
  
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(endpoint)) {
    setDisplayMode(DisplayMode::Error, "Login", "HTTP begin fail");
    return false;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Hermes-Device-Id", deviceId);
  http.addHeader("X-Hermes-Device-Name", HERMES_DEVICE_NAME);
  
  String payload = "{\"token\":\"" + escapeJson(deviceId) + 
                   "\",\"id\":\"" + escapeJson(deviceId) + 
                   "\",\"device_code\":\"" + escapeJson(deviceCode) + 
                   "\",\"device_type\":\"global_agent\",\"source\":\"global_agent\"" +
                   ",\"account\":\"" + escapeJson(account) + 
                   "\",\"password\":\"" + escapeJson(password) + 
                   "\",\"relayMode\":\"lan\"}";
  
  int code = http.POST(payload);
  String response = http.getString();
  http.end();
  
  Serial.printf("MCU login HTTP %d: %s\n", code, response.c_str());
  
  bool ok = code >= 200 && code < 300;
  if (ok) {
    mcuAuthToken = jsonStringValue(response, "token");
    mcuSocketRelayUrl = jsonStringValue(response, "relay_url");
    
    prefs.begin("mcu", false);
    prefs.putString("auth_token", mcuAuthToken);
    prefs.putString("active_url", activeDeviceUrl);
    prefs.putString("cur_account", account);
    prefs.putString("cur_password", password);
    prefs.putString("cur_profile", selectedProfile);
    if (mcuSocketRelayUrl.length() > 0) {
      prefs.putString("relay_url", mcuSocketRelayUrl);
    }
    prefs.end();
    
    connectMcuSocketClient();
    setDisplayMode(DisplayMode::Ready, WiFi.localIP().toString(), selectedProfile, 100);
  } else {
    setDisplayMode(DisplayMode::Error, "Login failed", String(code));
  }
  
  return ok;
}

// ============================================================
// Web 服务器
// ============================================================

String htmlEscape(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

void handleRoot() {
  if (wifiReady && WiFi.status() == WL_CONNECTED) {
    String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>HStudio XRS</title><style>body{font-family:sans-serif;max-width:400px;margin:20px auto;padding:0 15px}";
    html += "h1{font-size:1.5em}.info{background:#f0f0f0;padding:15px;border-radius:8px;margin:10px 0}";
    html += "input,select{width:100%;padding:10px;margin:5px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}";
    html += "button{width:100%;padding:12px;background:#007bff;color:#fff;border:none;border-radius:4px;font-size:16px;margin:5px 0}";
    html += "button.secondary{background:#6c757d}.btn-danger{background:#dc3545}</style></head><body>";
    html += "<h1>HStudio XRS</h1>";
    html += "<div class='info'><b>Device ID:</b> " + htmlEscape(deviceId) + "<br>";
    html += "<b>IP:</b> " + WiFi.localIP().toString() + "<br>";
    html += "<b>Profile:</b> " + htmlEscape(selectedProfile.length() > 0 ? selectedProfile : "Not set") + "</div>";
    
    if (mcuAuthToken.length() == 0) {
      html += "<h2>Login to Hermes Studio</h2>";
      html += "<form method='post' action='/login'>";
      html += "<input type='text' name='account' placeholder='Account' value='" + htmlEscape(savedAccount) + "' required>";
      html += "<input type='password' name='password' placeholder='Password' required>";
      String defaultUrl = discoveredGatewayUrl;
      if (defaultUrl.length() == 0) {
        defaultUrl = "http://" + WiFi.localIP().toString();
      }
      html += "<input type='text' name='url' placeholder='Studio URL (http://...)' value='" + defaultUrl + "' required>";
      html += "<input type='text' name='profile' placeholder='Profile name' value='" + htmlEscape(selectedProfile) + "' required>";
      html += "<button type='submit'>Login</button></form>";
    } else {
      html += "<p style='color:green'>Connected to Hermes Studio</p>";
      html += "<form method='post' action='/logout'><button type='submit' class='btn-danger'>Logout</button></form>";
    }
    
    html += "<h2>WiFi</h2><form method='post' action='/wifi'>";
    html += "<input type='text' name='ssid' placeholder='SSID' value='" + htmlEscape(savedSsid) + "'>";
    html += "<input type='password' name='pass' placeholder='Password'>";
    html += "<button type='submit'>Connect & Save</button></form>";
    html += "<form method='post' action='/clear'><button type='submit' class='secondary'>Clear WiFi & Enter Setup</button></form>";
    
    html += "<h2>Alarms</h2><form method='post' action='/alarm'>";
    html += "<input type='number' name='h' placeholder='Hour (0-23)' min='0' max='23'>";
    html += "<input type='number' name='m' placeholder='Minute (0-59)' min='0' max='59'>";
    html += "<button type='submit'>Add Alarm</button></form>";
    
    html += "<h2>Timer</h2><form method='post' action='/timer'>";
    html += "<input type='number' name='min' placeholder='Minutes' min='1' max='120'>";
    html += "<button type='submit'>Start Timer</button></form>";
    
    html += "</body></html>";
    webServer.send(200, "text/html; charset=utf-8", html);
  } else {
    String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>HStudio XRS Setup</title><style>body{font-family:sans-serif;max-width:400px;margin:20px auto;padding:0 15px}";
    html += "input{width:100%;padding:10px;margin:5px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}";
    html += "button{width:100%;padding:12px;background:#007bff;color:#fff;border:none;border-radius:4px;font-size:16px;margin:5px 0}";
    html += "</style></head><body>";
    html += "<h1>HStudio XRS Setup</h1><p>Connect to WiFi network:</p>";
    html += "<form method='post' action='/wifi'>";
    html += "<input type='text' name='ssid' placeholder='SSID' required>";
    html += "<input type='password' name='pass' placeholder='Password'>";
    html += "<button type='submit'>Connect</button></form>";
    html += "</body></html>";
    webServer.send(200, "text/html; charset=utf-8", html);
  }
}

void handleWifi() {
  String ssid = webServer.arg("ssid");
  ssid.trim();
  String pass = webServer.arg("pass");
  
  if (ssid.length() == 0) {
    webServer.send(400, "text/plain", "Missing SSID");
    return;
  }
  
  savedSsid = ssid;
  savedPass = pass;
  
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  
  if (connectWifi(ssid, pass)) {
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
    
    delay(1000);
    if (setupApMode) {
      WiFi.softAPdisconnect(true);
      setupApMode = false;
    }
  } else {
    webServer.send(200, "text/html", "<p>Connection failed. <a href='/'>Try again</a></p>");
  }
}

void handleLogin() {
  String account = webServer.arg("account");
  String password = webServer.arg("password");
  String url = webServer.arg("url");
  String profile = webServer.arg("profile");
  
  account.trim();
  url.trim();
  profile.trim();
  
  if (account.length() == 0 || url.length() == 0 || profile.length() == 0) {
    webServer.send(400, "text/plain", "Missing fields");
    return;
  }
  
  activeDeviceUrl = url;
  selectedProfile = profile;
  
  if (runMcuLogin(account, password)) {
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
  } else {
    webServer.send(200, "text/html", "<p>Login failed. <a href='/'>Try again</a></p>");
  }
}

void handleLogout() {
  mcuAuthToken = "";
  mcuSocketRelayUrl = "";
  // Keep selectedProfile - don't clear it
  disconnectMcuSocketClient();
  
  prefs.begin("mcu", false);
  prefs.remove("auth_token");
  prefs.remove("active_url");
  prefs.remove("relay_url");
  prefs.end();
  
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

void handleClear() {
  prefs.begin("wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
  
  webServer.send(200, "text/html", "<p>WiFi cleared. Entering setup mode...</p>");
  delay(1000);
  startSetupAp();
}

void handleAlarm() {
  String hStr = webServer.arg("h");
  String mStr = webServer.arg("m");
  
  if (hStr.length() > 0 && mStr.length() > 0) {
    uint8_t h = hStr.toInt();
    uint8_t m = mStr.toInt();
    
    for (uint8_t i = 0; i < kMaxAlarms; i++) {
      if (!alarms[i].enabled) {
        alarms[i].hour = h;
        alarms[i].minute = m;
        alarms[i].enabled = true;
        alarms[i].triggered = false;
        break;
      }
    }
  }
  
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

void handleTimer() {
  String minStr = webServer.arg("min");
  if (minStr.length() > 0) {
    uint32_t min = minStr.toInt();
    if (min > 0 && min <= 120) {
      timerDuration = min * 60;
      timerStartMs = millis();
      timerRunning = true;
    }
  }
  
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

// ============================================================
// AP 模式
// ============================================================

void startSetupAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HERMES_AP_SSID);
  setupApMode = true;
  Serial.printf("Setup AP started: %s IP=%s\n", HERMES_AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ============================================================
// 初始化
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("HStudio XRS - Hermes 学而思 ESP32版 v2.0");
  Serial.println("========================================");
  
  // 初始化设备 ID
  deviceId = getDeviceId();
  deviceCode = getDeviceCode();
  Serial.printf("Device ID: %s\n", deviceId.c_str());
  Serial.printf("Device Code: %s\n", deviceCode.c_str());
  
  // 初始化显示
  Serial.println("Initializing display...");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  Serial.printf("Display: %dx%d\n", TFT_WIDTH, TFT_HEIGHT);
  
  Serial.println("Display update starting...");
  setDisplayMode(DisplayMode::Boot);
  updateDisplay();
  Serial.println("Display update done!");
  
  // 初始化引脚
  pinMode(kPinKeyUp, INPUT_PULLUP);
  pinMode(kPinKeyDown, INPUT_PULLUP);
  pinMode(kPinKeyLeft, INPUT_PULLUP);
  pinMode(kPinKeyRight, INPUT_PULLUP);
  pinMode(kPinKeyA, INPUT_PULLUP);
  pinMode(kPinKeyB, INPUT_PULLUP);
  pinMode(14, OUTPUT);  // 蜂鸣器
  pinMode(32, ANALOG);  // 电池
  Serial.println("Pins initialized");
  
  // 初始化 I2C
  Wire.begin(21, 15);
  Wire.setClock(100000);
  Serial.println("I2C initialized");
  
  // 初始化 I2S
  Serial.println("Initializing I2S...");
  if (!initI2S()) {
    Serial.println("I2S init failed, continuing without audio");
  }
  Serial.println("I2S init done");
  
  // 加载保存的凭证
  Serial.println("Loading saved credentials...");
  prefs.begin("wifi", true);
  savedSsid = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();
  
  prefs.begin("mcu", true);
  mcuAuthToken = prefs.getString("auth_token", "");
  activeDeviceUrl = prefs.getString("active_url", "");
  mcuSocketRelayUrl = prefs.getString("relay_url", "");
  selectedProfile = prefs.getString("cur_profile", "default");
  savedAccount = prefs.getString("cur_account", "");
  if (selectedProfile.length() == 0) selectedProfile = "default";
  prefs.end();
  
  // 加载闹钟
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    String key = "alarm_" + String(i);
    prefs.begin("alarms", true);
    alarms[i].hour = prefs.getUInt((key + "_h").c_str(), 0);
    alarms[i].minute = prefs.getUInt((key + "_m").c_str(), 0);
    alarms[i].enabled = prefs.getBool((key + "_e").c_str(), false);
    prefs.end();
  }
  
  Serial.printf("Credentials loaded. SSID='%s' token=%d url=%d\n", 
                savedSsid.c_str(), mcuAuthToken.length(), activeDeviceUrl.length());
  
  // 尝试连接 WiFi
  if (savedSsid.length() > 0) {
    Serial.printf("Trying saved WiFi: %s\n", savedSsid.c_str());
    if (connectWifi(savedSsid, savedPass)) {
      if (mcuAuthToken.length() > 0 && activeDeviceUrl.length() > 0) {
        connectMcuSocketClient();
      }
    } else {
      Serial.println("WiFi connection failed, starting AP...");
      startSetupAp();
    }
  } else {
    Serial.println("No saved WiFi, starting AP...");
    startSetupAp();
  }
  
  // 启动 Web 服务器
  Serial.println("Starting web server...");
  webServer.on("/", handleRoot);
  webServer.on("/wifi", handleWifi);
  webServer.on("/login", handleLogin);
  webServer.on("/logout", handleLogout);
  webServer.on("/clear", handleClear);
  webServer.on("/alarm", handleAlarm);
  webServer.on("/timer", handleTimer);
  webServer.begin();
  
  Serial.println("Web server started");
  Serial.println("========================================");
  Serial.println("SETUP COMPLETE - entering main loop");
  Serial.println("========================================");
}

// ============================================================
// 主循环
// ============================================================

void loop() {
  // 处理 Web 请求
  webServer.handleClient();
  
  // 处理按键
  processButtons();
  
  // 处理 Socket.IO
  if (wsReady) {
    mcuSocketLoop();
  } else if (wifiReady && mcuAuthToken.length() > 0 && socketConnectFailures < 5) {
    if (millis() - lastSocketConnectAttempt > SOCKET_RECONNECT_INTERVAL_MS) {
      lastSocketConnectAttempt = millis();
      connectMcuSocketClient();
    }
  }
  
  // 定期任务
  static uint32_t lastPeriodic = 0;
  if (millis() - lastPeriodic > 1000) {
    lastPeriodic = millis();
    
    // 时间同步
    if (wifiReady && (!timeSynced || (time(nullptr) - lastNtpSync) > 3600)) {
      syncTime();
    }
    
    // 传感器读取
    readSensors();
    
    // 电池监测
    readBattery();
    
    // 闹钟检测
    checkAlarms();
    
    // 定时器检测
    if (timerRunning) {
      uint32_t elapsed = (millis() - timerStartMs) / 1000;
      if (elapsed >= timerDuration) {
        timerRunning = false;
        playBuzzer(1500, 2000);
        addNotification("Timer", "Time's up!");
      }
    }
    
    // 菜单超时返回时钟
    if (currentDisplayMode == DisplayMode::Menu && (millis() - lastMenuActivity) > kMenuTimeoutMs) {
      currentDisplayMode = DisplayMode::Clock;
      displayDirty = true;
    }
    
    // 通知显示超时
    if (currentDisplayMode == DisplayMode::Notification && (millis() - lastMenuActivity) > 10000) {
      currentDisplayMode = DisplayMode::Clock;
      displayDirty = true;
    }
    
    // 时钟模式自动刷新
    if (currentDisplayMode == DisplayMode::Clock && timeSynced) {
      static time_t lastClockUpdate = 0;
      time_t now;
      time(&now);
      if (now != lastClockUpdate) {
        lastClockUpdate = now;
        displayDirty = true;
      }
    }
  }
  
  // 更新显示
  if (displayDirty) {
    updateDisplay();
  }
  
  yield();
}
