# LilyGO T5 2.13" E-Paper Hermes Studio 小方盒

基于 Hermes Studio ESP32-C3 官方固件协议，适配 LilyGO T5 V2.3 2.13" E-Paper 硬件的替代实现。

## 硬件清单

| 组件 | 说明 | 参考价格 |
|------|------|----------|
| LilyGO T5 V2.3 2.13" E-Paper | ESP32 + 2.13寸墨水屏 | ¥45-60 |
| INMP441 麦克风模块 | I2S 数字麦克风 | ¥5-8 |
| MAX98357A 音频功放模块 | I2S D类功放 | ¥4-6 |
| 3.5mm 扬声器 | 3W 4Ω | ¥5-10 |
| 按键开关 | 6x6mm 轻触开关 | ¥0.5 |
| 杜邦线 | 母对母/公对母 | ¥3 |
| MicroSD 卡 | 可选，用于存储音频 | ¥10 |

## 引脚接线图

### LilyGO T5 引脚定义 (官方确认)

```
┌─────────────────────────────────────────────────────────────┐
│                    LilyGO T5 V2.3                           │
│                  2.13" E-Paper                              │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                 2.13" E-Ink 显示屏                   │   │
│  │              (DEPG0213BN, SSD1680)                  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│   ESP32 GPIO 分配:                                          │
│                                                             │
│   GPIO 5  ──── E-Paper CS  (片选)        ← 官方确认         │
│   GPIO 17 ──── E-Paper DC  (数据/命令)   ← 官方确认         │
│   GPIO 16 ──── E-Paper RST (复位)        ← 官方确认         │
│   GPIO 4  ──── E-Paper BUSY (忙信号)     ← 官方确认         │
│   GPIO 18 ──── E-Paper SCK (时钟, 板上已连接)              │
│   GPIO 23 ──── E-Paper SDA (数据, 板上已连接)              │
│                                                             │
│   GPIO 27 ──── I2S BCK  (位时钟)                           │
│   GPIO 26 ──── I2S WS   (左右声道选择)                     │
│   GPIO 34 ──── I2S DIN  (ADC/麦克风数据输入)               │
│   GPIO 25 ──── I2S DOUT (DAC/功放数据输出)                 │
│                                                             │
│   GPIO 0  ──── BOOT 按键 (内置)                            │
│                                                             │
│   ⚠️ V2.3.1 版本无 EPD_PWR_EN (GPIO12) 引脚                │
│      显示电源始终开启，无需控制                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 完整接线图

```text
                    ┌──────────────────┐
                    │   LilyGO T5      │
                    │   V2.3           │
                    │                  │
                    │  ┌────────────┐  │
                    │  │ 2.13" E-Ink│  │
                    │  │  显示屏     │  │
                    │  │DEPG0213BN  │  │
                    │  └────────────┘  │
                    │                  │
                    │  ESP32           │
                    │                  │
                    │  GPIO5  ─────────┼──── E-Paper CS
                    │  GPIO17 ─────────┼──── E-Paper DC
                    │  GPIO16 ─────────┼──── E-Paper RST
                    │  GPIO4  ─────────┼──── E-Paper BUSY
                    │                  │
                    │  GPIO27 ─────────┼──── I2S BCK ──┐
                    │  GPIO26 ─────────┼──── I2S WS   ─┤
                    │  GPIO34 ─────────┼──── I2S DIN  ─┤
                    │  GPIO25 ─────────┼──── I2S DOUT ─┤
                    │                  │                │
                    │  GPIO0  ─────────┼──── BOOT 按键 │
                    │                  │                │
                    │  3.3V   ─────────┼──── 电源正    │
                    │  GND    ─────────┼──── 电源地    │
                    └──────────────────┘                │
                                                        │
        ┌───────────────────────────────────────────────┘
        │
        │   ┌─────────────────────┐
        │   │   INMP441 麦克风     │
        │   │                     │
        │   │  VDD ──── 3.3V      │
        │   │  GND ──── GND       │
        │   │  SCK ──── GPIO27    │ (I2S BCK)
        │   │  WS  ──── GPIO26    │ (I2S WS)
        │   │  SD  ──── GPIO34    │ (I2S DIN)
        │   │  L/R ──── GND       │ (左声道)
        │   └─────────────────────┘
        │
        │   ┌─────────────────────┐
        │   │  MAX98357A 功放     │
        │   │                     │
        │   │  VIN ──── 3.3V/5V   │
        │   │  GND ──── GND       │
        │   │  DIN ──── GPIO25    │ (I2S DOUT)
        │   │  BCLK──── GPIO27    │ (I2S BCK)
        │   │  LRC ──── GPIO26    │ (I2S WS)
        │   │  SD  ──── 3.3V      │ (使能)
        │   └─────────────────────┘
        │
        │   ┌─────────────────────┐
        │   │    3.5mm 扬声器      │
        │   │                     │
        │   │  + ──── MAX98357A + │
        │   │  - ──── MAX98357A - │
        │   └─────────────────────┘
        │
        │   ┌─────────────────────┐
        │   │    BOOT 按键         │
        │   │                     │
        │   │  一端 ──── GPIO0     │
        │   │  另一端 ─── GND      │
        │   └─────────────────────┘
        │
        └───────────────────────────────────────────
```

### 简化版（无外部音频模块）

如果不需要语音功能，仅实现显示和按键控制：

```
┌─────────────────────────────────────────────────────────────┐
│                    LilyGO T5 V2.3                           │
│                                                             │
│  板上已集成:                                                 │
│  - 2.13" E-Paper 显示屏 (SPI, DEPG0213BN)                   │
│  - BOOT 按键 (GPIO0)                                        │
│  - ESP32 WiFi 模块                                          │
│                                                             │
│  只需连接电源 (USB 或电池) 即可工作!                         │
│                                                             │
│  可选扩展:                                                   │
│  - INMP441 麦克风 (3.3V, GND, GPIO27, GPIO26, GPIO34)      │
│  - MAX98357A 功放 + 扬声器 (3.3V, GND, GPIO25, GPIO27, GPIO26)│
└─────────────────────────────────────────────────────────────┘
```

### 驱动确认

| 项目 | 确认值 | 来源 |
|------|--------|------|
| 显示面板 | DEPG0213BN (SSD1680) | LilyGO 官方 Factory.ino |
| 驱动类 | `GxEPD2_213_BN` | GxEPD2 库 |
| 分辨率 | 122 x 250 | 实测 |
| 驱动库 | `zinggjm/GxEPD2@^1.6.0` | PlatformIO |

## 固件编译与烧录

### 环境准备

1. 安装 [PlatformIO](https://platformio.org/) 或 VS Code + PlatformIO 插件
2. 安装 [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) 框架

### 编译

```bash
cd lilygo-t5
pio run
```

### 烧录

```bash
pio run -t upload
```

### 监控串口

```bash
pio device monitor
```

## 使用流程

### 1. 首次启动

1. 上电后，E-Paper 显示 "BOOT - starting"
2. 如果没有保存的 WiFi 配置，自动进入配网模式
3. 手机连接 "HStudio-T5" WiFi 热点
4. 浏览器打开 192.168.4.1
5. 选择 WiFi 网络并输入密码
6. 设备自动重启并连接 WiFi

### 2. 配对 Hermes Studio

1. 确保 Hermes Studio 在电脑/服务器上运行
2. 浏览器打开设备 IP 地址
3. 在设备页面输入:
   - Device Key: Hermes Studio 生成的设备密钥
   - Device URL: Hermes Studio 的 WebSocket URL
   - Token: 认证令牌
   - Profile: 配置文件名
4. 点击 "Login" 完成配对

### 3. 日常使用

| 操作 | 功能 |
|------|------|
| 长按 BOOT 按键 (>360ms) | 开始语音对话 |
| 单击 BOOT 按键 | 停止当前播放 |
| 双击 BOOT 按键 | 清除会话 |

### 4. E-Paper 显示状态

| 显示 | 状态 |
|------|------|
| BOOT - starting | 启动中 |
| WIFI - CONNECT | 连接 WiFi 中 |
| READY - [IP] | 已连接，等待操作 |
| THINK - ... | AI 思考中 |
| RECORD - Listening... | 语音录制中 |
| PLAY - Playing... | 语音播放中 |
| ERROR - ... | 错误状态 |

## 协议实现对照

| 功能 | 官方固件 | 本实现 | 状态 |
|------|----------|--------|------|
| WiFi 连接 | ✅ | ✅ | 完整 |
| AP 配网模式 | ✅ | ✅ | 完整 |
| 设备发现 (mDNS) | ✅ | ❌ | 简化 (手动输入) |
| Socket.IO 连接 | ✅ | ✅ | 完整 |
| mcu.ready 事件 | ✅ | ✅ | 完整 |
| mcu.interaction 事件 | ✅ | ✅ | 完整 |
| mcu.audio 播放 | ✅ | ✅ | 完整 |
| 语音录制 | ✅ | ✅ | 完整 |
| 语音上传 | ✅ | ✅ | 完整 |
| E-Paper 显示 | ✅ (OLED) | ✅ | 完整 |
| 按键控制 | ✅ | ✅ | 完整 |
| OTA 更新 | ✅ | ❌ | 未实现 |
| 自动 VAD 监听 | ✅ | ❌ | 未实现 |
| 低功耗待机 | ✅ | ❌ | 未实现 |
| 多 Profile | ✅ | ✅ | 完整 |

## 与官方硬件的差异

| 特性 | 官方 ESP32-C3 小方盒 | LilyGO T5 实现 |
|------|----------------------|----------------|
| 主控 | ESP32-C3 | ESP32 (经典) |
| 显示屏 | 128x64 OLED | 250x122 E-Paper (DEPG0213BN, SSD1680) |
| 音频编解码器 | ES8311 (内置) | 需外接 INMP441 + MAX98357A |
| 麦克风 | 内置 MEMS | 外接 INMP441 |
| 扬声器 | 内置 | 外接 |
| 按键 | 2个 | 1个 (BOOT) |
| 电池管理 | 内置 | 无 |
| 外壳 | 3D 打印 | 需自行设计 |
| 价格 | ¥99 | ¥60-80 (散件) |

## 二次开发

### 添加更多按键

```cpp
// 在 main.cpp 中添加
#define KEY_PIN 14

void setup() {
  pinMode(KEY_PIN, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(KEY_PIN) == LOW) {
    // 按键按下
  }
}
```

### 修改显示内容

```cpp
// 自定义显示
display.setFullWindow();
display.firstPage();
do {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, 30);
  display.print("Hello Hermes!");
} while (display.nextPage());
```

### 添加传感器

```cpp
// 例如添加 DHT11 温湿度传感器
#include <DHT.h>
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  dht.begin();
}

float temp = dht.readTemperature();
float humi = dht.readHumidity();
```

## 故障排除

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 屏幕无显示 | 接线错误 | 检查 SPI 接线 |
| 无法连接 WiFi | 密码错误 | 重新配网 |
| Socket.IO 连接失败 | Studio 未运行 | 检查 Studio 状态 |
| 无声音 | 音频模块未连接 | 检查 I2S 接线 |
| 录音失败 | 麦克风接线错误 | 检查 INMP441 接线 |
| 设备反复重启 | 电源不足 | 使用 5V 2A 电源 |

## 参考资料

- [Hermes Studio 官方文档](https://hermes-studio.ai/docs/)
- [LilyGO T5 GitHub](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [GxEPD2 库文档](https://github.com/ZinggJM/GxEPD2)
- [ESP32 Arduino 文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [Hermes Agent 源码](https://github.com/NousResearch/hermes-agent)
- [Hermes Studio 源码](https://github.com/EKKOLearnAI/hermes-studio)

## 许可证

MIT License - 基于 Hermes Studio 开源协议
