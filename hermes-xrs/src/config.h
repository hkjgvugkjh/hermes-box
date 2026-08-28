/**
 * @file config.h
 * @brief Hermes 学而思 ESP32版 (hermes-xrs) 引脚配置
 * 
 * 主控：ESP32-S3
 * 显示：1.8" SPI TFT (ST7735 128x160)
 * 存储：MicroSD 卡 (SPI2, 与 TFT 共享)
 * 按键：6 键 (上/下/左/右/A/B)
 * 蜂鸣器：GPIO14 (无源, PWM 驱动)
 * 传感器：光照 (GPIO36)、热敏 (GPIO39)
 * 总线：I2C (GPIO15/21)、UART0 (GPIO1/3)
 * 
 * 与 hermes-t5 功能完全相同：
 * - 语音对话 (Socket.IO + ADPCM)
 * - 音频播放 (I2S + HTTP流)
 * - 时钟/日期显示 (NTP同步)
 * - 天气显示 (HTTP API)
 * - 环境传感器 (光照/热敏)
 * - 通知系统 (消息队列)
 * - 菜单系统 (模式切换)
 * - 多按键手势 (单击/双击/长按)
 * - 定时器/闹钟
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

// ============================================================
// 按键 (6键，低电平有效，内部上拉)
// ============================================================
#define KEY_UP      2
#define KEY_DOWN    13
#define KEY_LEFT    27
#define KEY_RIGHT   35
#define KEY_A       34      // 仅输入
#define KEY_B       12      // 启动敏感，上电避免外部高电平

// ============================================================
// TFT 显示屏 (SPI2 - HSPI)
// ============================================================
#define TFT_SCK     18      // 与 SD 卡共享
#define TFT_MOSI    23      // 与 SD 卡共享
#define TFT_MISO    19      // 与 SD 卡 MISO 共享
#define TFT_CS      5
#define TFT_DC      4
#define TFT_RES     19      // 与 SD 卡 MISO 共享（注意：可能需要调整）
#define TFT_LED     -1      // 背光（-1 = 未连接）

// TFT 参数
#define TFT_WIDTH   128
#define TFT_HEIGHT  160
#define TFT_DRIVER  ST7735  // 或 ILI9341

// ============================================================
// MicroSD 卡 (SPI2 - HSPI，与 TFT 共享 SCK/MOSI/MISO)
// ============================================================
#define SD_SCK      18      // 共享
#define SD_MOSI     23      // 共享
#define SD_MISO     19      // 共享
#define SD_CS       22

// ============================================================
// 无源蜂鸣器 (PWM - LEDC)
// ============================================================
#define BUZZER_PIN      14
#define BUZZER_CHANNEL  0
#define BUZZER_FREQ     2000    // 默认频率 Hz
#define BUZZER_RESOLUTION 8     // 8-bit 分辨率

// ============================================================
// 传感器 (ADC1，仅输入)
// ============================================================
#define SENSOR_LIGHT    36      // ADC1_CH0，仅输入
#define SENSOR_TEMP     39      // ADC1_CH3，仅输入

// ============================================================
// I2C 总线
// ============================================================
#define I2C_SCL     15
#define I2C_SDA     21
#define I2C_FREQ    400000  // 400kHz

// ============================================================
// I2S 音频
// ============================================================
#define I2S_BCK     27
#define I2S_WS      26
#define I2S_DIN     34
#define I2S_DOUT    25

// ============================================================
// 电池检测
// ============================================================
#define BATTERY_ADC 32
#define BATTERY_DIVIDER_RATIO 2.0
#define BATTERY_MAX_VOLTAGE 4.2
#define BATTERY_MIN_VOLTAGE 3.3

// ============================================================
// UART0 (原生串口，不经过 USB)
// ============================================================
#define UART0_TX    1
#define UART0_RX    3

// ============================================================
// 预留扩展引脚
// ============================================================
#define EXT_PIN_1   33
#define EXT_PIN_2   32
#define EXT_PIN_3   26
#define EXT_PIN_4   25

// ============================================================
// 关键限制
// ============================================================
// GPIO34, 35, 36, 39 = 仅输入（不可输出）
// GPIO12 = 启动敏感（B 键）
// GPIO18/23/19 = 共享 SPI（TFT/SD 分时复用）
// I2C 地址 0x40 = 电机/LED 共用

// ============================================================
// Hermes Studio 配置
// ============================================================
#define HERMES_DEVICE_NAME  "HStudio-XRS"
#define HERMES_AP_SSID      "HStudio-XRS"
#define HERMES_HTTP_PORT    80
#define HERMES_FIRMWARE_VER "v2.0"

// Socket.IO 配置
#define SOCKET_RECONNECT_INTERVAL_MS  5000
#define SOCKET_MAX_FRAME_SIZE         8192
#define WIFI_CONNECT_TIMEOUT_MS       15000

// 音频配置
#define VOICE_SAMPLE_RATE     16000
#define VOICE_RECORD_MAX_MS   15000
#define VOICE_RECORD_MIN_MS   500
#define VOICE_VAD_THRESHOLD   300

// NTP 配置
#define NTP_SERVER        "pool.ntp.org"
#define GMT_OFFSET_SEC    (8 * 3600)  // UTC+8
#define DAYLIGHT_OFFSET_SEC 0

// 显示状态
enum class DisplayMode {
  Boot,
  WifiSetup,
  WifiConnecting,
  Login,
  Ready,
  Think,
  Listen,
  Play,
  Error,
  Clock,
  Weather,
  Sensors,
  Music,
  Notification,
  Menu,
  Timer,
  Alarm
};

#endif // _CONFIG_H_
