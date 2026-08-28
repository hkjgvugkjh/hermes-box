# Hermes 学而思 ESP32版 (hermes-xrs)

基于 ESP32-S3 + 1.8" SPI TFT 彩屏的 Hermes Studio 语音助手方案，与 hermes-t5 墨水屏版功能完全相同。

## 硬件清单

| 组件 | 说明 | 参考价格 |
|------|------|----------|
| ESP32-S3-DevKitC-1 | ESP32-S3 + 8MB Flash + 8MB PSRAM | ¥35-45 |
| 1.8" SPI TFT (ST7735) | 128×160 彩色显示屏 | ¥12-18 |
| INMP441 麦克风模块 | I2S 数字麦克风 | ¥5-8 |
| MAX98357A 音频功放模块 | I2S D类功放 | ¥4-6 |
| 3.5mm 扬声器 | 3W 4Ω | ¥5-10 |
| 按键开关 | 6x6mm 轻触开关 ×6 | ¥3 |
| 无源蜂鸣器 | 3.3V | ¥1 |
| 杜邦线 | 母对母/公对母 | ¥3 |
| MicroSD 卡 | 可选，用于存储 | ¥10 |

## 引脚接线图

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3                                 │
│                                                             │
│   SPI TFT (ST7735 128x160):                                 │
│     GPIO 18 ──── SCK  (时钟，与 SD 共享)                     │
│     GPIO 23 ──── MOSI (数据，与 SD 共享)                     │
│     GPIO 19 ──── MISO (数据，与 SD 共享)                     │
│     GPIO 5  ──── CS   (片选)                                │
│     GPIO 4  ──── DC   (数据/命令)                           │
│     GPIO 19 ──── RES  (复位，与 MISO 共用)                   │
│                                                             │
│   MicroSD 卡:                                                │
│     GPIO 18 ──── SCK  (共享)                                │
│     GPIO 23 ──── MOSI (共享)                                │
│     GPIO 19 ──── MISO (共享)                                │
│     GPIO 22 ──── CS   (独立片选)                             │
│                                                             │
│   I2S 音频:                                                  │
│     GPIO 27 ──── BCK  (位时钟)                              │
│     GPIO 26 ──── WS   (左右声道选择)                         │
│     GPIO 34 ──── DIN  (ADC/麦克风数据输入)                   │
│     GPIO 25 ──── DOUT (DAC/功放数据输出)                     │
│                                                             │
│   按键 (低电平有效，内部上拉):                               │
│     GPIO 2  ──── 上                                         │
│     GPIO 13 ──── 下                                         │
│     GPIO 27 ──── 左                                         │
│     GPIO 35 ──── 右                                         │
│     GPIO 34 ──── A 键 (确认/播放)                           │
│     GPIO 12 ──── B 键 (返回/停止)                           │
│                                                             │
│   蜂鸣器:                                                    │
│     GPIO 14 ──── 无源蜂鸣器 (PWM)                            │
│                                                             │
│   传感器:                                                    │
│     GPIO 36 ──── 光照传感器 (ADC1_CH0)                      │
│     GPIO 39 ──── 热敏传感器 (ADC1_CH3)                      │
│     GPIO 32 ──── 电池电压 ADC (可选)                         │
│                                                             │
│   I2C:                                                       │
│     GPIO 15 ──── SCL                                        │
│     GPIO 21 ──── SDA                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 功能特性

与 hermes-t5 墨水屏版 **完全相同** 的功能：

- 🗣️ **语音对话** — Socket.IO + ADPCM 编码，实时语音交互
- 🔊 **音频播放** — I2S + HTTP 流媒体播放
- 🕐 **时钟显示** — NTP 自动同步，大字体时间日期
- 🌤️ **天气显示** — wttr.in API 实时天气
- 🌡️ **环境传感器** — 光照/热敏 ADC 采集
- 🔋 **电池监测** — ADC 检测电池电压
- 🔔 **通知系统** — 消息队列，蜂鸣器提醒
- ⏰ **闹钟** — 最多 5 个闹钟，Web 设置
- ⏱️ **定时器** — 1-120 分钟，Web 设置
- 🎵 **音乐播放** — 播放/暂停/下一首/进度显示
- 📋 **菜单系统** — 8 个功能菜单，按键导航
- 🔘 **多按键手势** — 单击/双击/长按，6 个按键

## 编译与烧录

### 环境准备

```bash
pip install platformio
# 或
brew install platformio
```

### 编译

```bash
cd hermes-xrs
pio run
```

### 烧录

```bash
pio run -t upload
```

### 监控串口

```bash
pio device monitor -b 115200
```

## 使用指南

### 首次配置

1. **上电** — 设备进入 AP 配网模式
2. **连接热点** — 手机连接 `HStudio-XRS`
3. **配置 WiFi** — 浏览器打开 `192.168.4.1`
4. **网关发现** — 设备自动扫描局域网内的 Hermes Studio 网关
5. **登录认证** — 在网页输入 Hermes Studio 账号密码

### 按键操作

| 按键 | 单击 | 双击 | 长按 |
|------|------|------|------|
| **上** | 上一项 | - | 音量加 |
| **下** | 下一项 | - | 音量减 |
| **左** | - | - | - |
| **右** | - | - | - |
| **A** | 确认/进入菜单 | - | 语音对话 |
| **B** | 播放/暂停 | - | 停止/返回 |

### 显示模式

| 模式 | 说明 |
|------|------|
| **Clock** | 大字体时钟 + 日期 + 传感器数据 |
| **Weather** | 天气信息 |
| **Sensors** | 温度/光照/电池 |
| **Music** | 播放控制 + 进度条 |
| **Messages** | 通知消息列表 |
| **Timer** | 倒计时显示 |
| **Alarm** | 闹钟列表 |
| **Menu** | 功能菜单 |

## 与 T5 版本对比

| 特性 | hermes-t5 (墨水屏) | hermes-xrs (彩屏) |
|------|-------------------|-------------------|
| **主控** | ESP32-D0WDQ6 | ESP32-S3 |
| **显示屏** | 2.13" E-Paper (DEPG0213BN) | 1.8" SPI TFT (ST7735) |
| **分辨率** | 122×250 (黑白) | 128×160 (65K色) |
| **刷新率** | 4秒/页 | 实时刷新 |
| **功耗** | 超低功耗 (μA级) | 较高 (mA级) |
| **室内可读性** | 极好 (反射式) | 一般 (背光) |
| **按键** | 4个 | 6个 |
| **传感器** | SHT30 (I2C) | 光照 + 热敏 (ADC) |
| **存储** | 无 | MicroSD |
| **适用场景** | 时钟/通知常驻显示 | 交互频繁/彩色需求 |

## 二次开发

### 自定义显示

```cpp
// 在 updateDisplay() 中添加自定义模式
tft.fillScreen(TFT_BLACK);
tft.setTextColor(TFT_WHITE);
tft.setCursor(10, 10);
tft.print("Hello XRS!");
```

### 添加传感器

```cpp
// I2C 传感器 (SHT30)
#include <Wire.h>
#define SHT30_ADDR 0x44

void readSHT30() {
  Wire.beginTransmission(SHT30_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  Wire.endTransmission();
  delay(15);
  Wire.requestFrom(SHT30_ADDR, 6);
  // 解析温度湿度...
}
```

## 故障排除

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕无显示 | 接线错误 / 驱动不匹配 | 检查 SPI 接线，确认 ST7735 驱动 |
| 无法连接 WiFi | 密码错误 / 信号弱 | 重新配网 |
| Socket.IO 连接失败 | 网关未运行 | 检查 Hermes Studio 状态 |
| auth.invalid 循环 | Token 过期 | 清除 token 重新登录 |
| 无声音 | I2S 接线错误 | 检查 BCK/WS/DIN/DOUT |
| 录音失败 | 麦克风接线错误 | 检查 INMP441 接线 |
| 屏幕白屏 | TFT 初始化失败 | 检查 CS/DC 接线 |

## 参考资料

- [Hermes Studio 官方](https://github.com/EKKOLearnAI/hermes-studio)
- [ESP32 Arduino 文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [TFT_eSPI 库](https://github.com/Bodmer/TFT_eSPI)
- [PlatformIO](https://platformio.org/)

## 许可证

MIT License - 基于 Hermes Studio 开源协议
