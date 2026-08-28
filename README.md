# Hermes Studio 小方盒 — 完整项目

## 📋 项目概述

**Hermes 小方盒** 是基于 ESP32 开源硬件的语音助手方案，通过 Socket.IO 协议连接 [Hermes Studio](https://github.com/EKKOLearnAI/hermes-studio) 网关，实现语音对话、音频播放、环境监测等功能。

本项目包含三个独立子项目：

| 子项目 | 说明 | 版本 |
|--------|------|------|
| [hermes-device](hermes-device/) | 独立协议库 (C++ header-only) | v1.0.0 |
| [hermes-t5](hermes-t5/) | LilyGO T5 2.13" E-Paper 墨水屏版本 | v2.2 |
| [hermes-xrs](hermes-xrs/) | ESP32-S3 + SPI TFT 彩屏版本 | v2.2 |

---

## 🏗️ 项目结构

```
hermes-box/
├── hermes-device/              # 📦 独立协议库
│   ├── src/
│   │   ├── hermes-device.h     # 库头文件 (~450行)
│   │   └── hermes-device.cpp   # 库实现 (~1150行)
│   ├── library.properties      # Arduino Library 配置
│   └── README.md
│
├── hermes-t5/                  # 🖥️ 墨水屏版本
│   ├── src/main.cpp            # T5 固件 (~2295行)
│   ├── platformio.ini          # PlatformIO 配置
│   ├── partitions.csv          # 分区表
│   └── README.md
│
├── hermes-xrs/                 # 📱 彩屏版本
│   ├── src/
│   │   ├── main.cpp            # XRS 固件 (~2207行)
│   │   └── config.h            # 引脚配置
│   ├── platformio.ini
│   └── README.md
│
└── .gitmodules                 # Git 子模块配置
```

---

## 🔌 硬件对比

| 特性 | hermes-t5 (墨水屏) | hermes-xrs (彩屏) |
|------|-------------------|-------------------|
| **主控** | ESP32-D0WDQ6 | ESP32-S3 |
| **显示屏** | 2.13" E-Paper (DEPG0213BN) | 1.8" SPI TFT (ST7735) |
| **分辨率** | 122×250 (黑白) | 128×160 (65K色) |
| **麦克风** | INMP441 (I2S) | INMP441 (I2S) |
| **功放** | MAX98357A (I2S) | MAX98357A (I2S) |
| **按键** | 4个 (BOOT + 3扩展) | 6个 (方向键 + A/B) |
| **传感器** | SHT30 (I2C温湿度) | 光照 + 热敏 (ADC) |
| **存储** | 无 | MicroSD 卡 |
| **电池** | 支持 (ADC监测) | 不支持 |
| **特色** | 超低功耗、类纸显示 | 彩色交互、按键丰富 |

---

## 📡 通信协议

### 协议栈

```
┌─────────────────────────────────────────┐
│           Hermes Studio 网关             │
│         (Node.js + Socket.IO)            │
├─────────────────────────────────────────┤
│  Engine.IO v4  ← 传输层 (WebSocket)     │
├─────────────────────────────────────────┤
│  Socket.IO    ← 事件层 (namespace)      │
├─────────────────────────────────────────┤
│  MCU Protocol ← 应用层 (JSON events)    │
└─────────────────────────────────────────┘
```

### 连接流程

```
设备                    Hermes Studio Gateway
 │                              │
 │──── HTTP POST /api/auth/mcu-login ────→│
 │←─── { token: "xxx" } ─────────────────│
 │                              │
 │──── WebSocket Upgrade ────────→│
 │←─── 101 Switching ───────────│
 │                              │
 │──── 40/global-agent (connect) →│
 │←─── 40/global-agent (ready) ──│
 │                              │
 │←─── 42["mcu.auth",{...}] ────│
 │──── 42["mcu.auth.ok",{...}] ─→│
 │──── 42["mcu.status",{...}] ──→│
 │                              │
 │  ✅ 连接建立，等待交互          │
```

### 核心事件

| 事件 | 方向 | 说明 |
|------|------|------|
| `mcu.auth` | 网关→设备 | 认证请求 |
| `mcu.auth.ok` | 设备→网关 | 认证响应 |
| `mcu.status` | 设备→网关 | 状态上报 |
| `mcu.ready` | 设备→网关 | 就绪通知 |
| `interaction.status` | 网关→设备 | 交互状态 |
| `audio.enqueue` | 网关→设备 | 音频播放 |
| `voice.stream.start` | 设备→网关 | 语音流开始 |
| `voice.stream.chunk` | 设备→网关 | 语音数据块 |
| `voice.stream.end` | 设备→网关 | 语音流结束 |
| `mcu.session.clear` | 网关→设备 | 清除会话 |
| `auth.invalid` | 网关→设备 | 认证失效 |

### 语音流格式

- **编码**: IMA-ADPCM 4-bit
- **采样率**: 16kHz
- **声道**: 单声道
- **帧格式**: `hadp-chunk-v1`
- **传输**: WebSocket binary frame (opcode 0x2)

---

## 🛠️ 开发环境

### 安装 PlatformIO

```bash
pip install platformio
# 或
brew install platformio
```

### 编译固件

```bash
# T5 版本
cd hermes-t5
pio run

# XRS 版本
cd hermes-xrs
pio run
```

### 烧录

```bash
# 自动检测串口
pio run -t upload

# 指定串口
pio run -t upload --upload-port /dev/ttyUSB0
```

### 监控串口

```bash
pio device monitor -b 115200
```

---

## 📖 使用指南

### 首次配置

1. **上电** — 设备进入 AP 配网模式
2. **连接热点** — 手机连接 `HStudio-T5` 或 `HStudio-XRS`
3. **配置 WiFi** — 浏览器打开 `192.168.4.1`，输入 WiFi 密码
4. **网关发现** — 设备自动扫描局域网内的 Hermes Studio 网关
5. **登录认证** — 在网页输入 Hermes Studio 账号密码

### 日常操作

#### T5 版本 (墨水屏)

| 按键 | 单击 | 双击 | 长按 |
|------|------|------|------|
| **BOOT** | 进入菜单 | - | 语音对话 |
| **KEY1** | 上一项 | - | 音量减 |
| **KEY2** | 下一项 | - | 音量加 |
| **KEY3** | 播放/暂停 | - | 停止 |

#### XRS 版本 (彩屏)

| 按键 | 功能 |
|------|------|
| **上/下** | 菜单导航 |
| **左/右** | 音量/亮度调节 |
| **A** | 确认/播放/暂停 |
| **B** | 返回/停止 |

### 显示模式

| 模式 | 说明 |
|------|------|
| **Clock** | 大字体时钟 + 日期 + 传感器数据 |
| **Weather** | 天气信息 (wttr.in API) |
| **Sensors** | 温湿度 + 电池电压 |
| **Music** | 播放控制 + 进度条 |
| **Messages** | 通知消息列表 |
| **Timer** | 倒计时显示 |
| **Alarm** | 闹钟列表 |
| **Menu** | 功能菜单 |

---

## 🔧 引脚定义

### T5 版本 (官方确认)

```
E-Paper:  CS=5, DC=17, RST=16, BUSY=4, SCK=18, MOSI=23
I2S:      BCK=27, WS=26, DIN=34, DOUT=25
I2C:      SDA=21, SCL=22 (SHT30传感器)
按键:     BOOT=0, KEY1=13, KEY2=14, KEY3=15
蜂鸣器:   GPIO2
电池ADC:  GPIO32
LED:      GPIO12
```

### XRS 版本

```
TFT:      SCK=18, MOSI=23, MISO=19, CS=5, DC=4
SD卡:     SCK=18, MOSI=23, MISO=19, CS=22 (共享SPI)
按键:     UP=2, DOWN=13, LEFT=27, RIGHT=35, A=34, B=12
蜂鸣器:   GPIO14 (PWM)
传感器:   光照=36, 热敏=39 (ADC1)
I2C:      SCL=15, SDA=21
```

---

## 📚 hermes-device 库 API

### 快速开始

```cpp
#include <hermes-device.h>

HermesDevice device;

void setup() {
  Serial.begin(115200);
  
  // 初始化设备
  device.begin("HStudio-Device", "global_agent");
  
  // 连接 WiFi
  device.connectWifi("YourSSID", "YourPassword");
  
  // 设置事件回调
  device.onInteraction([](const String& id, const String& status, const String& text) {
    Serial.printf("Interaction %s: %s\n", id.c_str(), status.c_str());
  });
}

void loop() {
  device.loop();  // 必须调用
}
```

### 主要 API

```cpp
// 初始化
void begin(const String& deviceName = "HStudio-Device", 
           const String& deviceType = "global_agent",
           const String& namespaceName = "/global-agent");

// WiFi
bool connectWifi(const String& ssid, const String& pass, bool save = true);
bool connectSavedWifi();
void disconnectWifi();
bool isWifiConnected() const;
void startSetupAp(const String& ssid, const String& password = "");

// 网关发现
String discoverGateway();
bool testGateway(const String& ip);
void setGatewayUrl(const String& url);

// 认证
bool login(const String& account, const String& password, const String& profile);
void logout();
bool isAuthenticated() const;

// 语音交互
bool startVoiceInteraction(const String& interactionId);
bool sendVoiceChunk(const String& interactionId, const uint8_t* data, size_t length, uint32_t offset);
bool endVoiceInteraction(const String& interactionId, uint32_t totalBytes);

// 状态上报
void reportStatus(const String& interactionId = "", const String& status = "", 
                  bool audioPlaying = false, uint32_t queueLength = 0);
void reportReady();

// 闹钟
bool addAlarm(uint8_t hour, uint8_t minute);
void removeAlarm(uint8_t index);
void toggleAlarm(uint8_t index, bool enabled);

// 定时器
void startTimer(uint32_t seconds);
void stopTimer();
bool isTimerRunning() const;
uint32_t getTimerRemaining() const;

// 传感器
void setSensorData(float temp, float humidity, float batteryV, uint8_t batteryPct);

// 时间同步
bool syncTime(const char* ntpServer = "pool.ntp.org", long gmtOffset = 8*3600);

// 主循环
void loop();
```

---

## 🔬 协议研究笔记

### 关键发现

1. **Engine.IO v4 握手**
   - 必须发送 HTTP WebSocket 升级请求
   - 等待 `101 Switching Protocols` 响应
   - 响应读取到空行 (`\r\n\r\n`) 为止
   - 等待 hello 包 (0{...}) 后再发送 namespace connect

2. **Socket.IO 认证**
   - 每个事件 JSON 必须包含 `apiToken` 字段
   - `mcu.auth.ok` 必须以事件帧格式发送
   - Token 过期后网关发送 `auth.invalid`

3. **语音流传输**
   - 使用 WebSocket binary frame (opcode 0x2)
   - 文本帧格式: `451-/global-agent,["voice.stream.chunk",{...}]`
   - 紧跟 binary frame 包含实际 ADPCM 数据

4. **网关发现**
   - mDNS 查询 `_http._tcp` 服务
   - 端口 8648 为 Hermes Studio 默认端口
   - 支持网络扫描作为后备方案

### 调试技巧

```bash
# 查看网关健康状态
curl http://10.10.168.101:8648/api/hermes/health

# 监控 WebSocket 流量
wscat -c ws://10.10.168.101:8648/socket.io/?EIO=4&transport=websocket

# 查看 mDNS 服务
avahi-browse -r _http._tcp
```

---

## 🐛 故障排除

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕无显示 | 接线错误 / 驱动不匹配 | 检查 CS/DC/RST/BUSY 引脚 |
| 无法连接 WiFi | 密码错误 / 信号弱 | 重新配网 |
| Socket.IO 连接失败 | 网关未运行 | 检查 Hermes Studio 状态 |
| auth.invalid 循环 | Token 过期 / 格式错误 | 清除 token 重新登录 |
| 无声音 | I2S 接线错误 | 检查 BCK/WS/DIN/DOUT |
| 录音失败 | 麦克风接线错误 | 检查 INMP441 接线 |
| 设备反复重启 | 电源不足 | 使用 5V 2A 电源 |

---

## 📦 发布历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v2.2 | 2026-08-28 | 修复 Engine.IO 握手、auth.invalid 处理、namespace 错误处理 |
| v2.1 | 2026-08-28 | 修复 WebSocket 握手、添加连接失败检测 |
| v2.0 | 2026-08-28 | 完整功能：语音、时钟、天气、传感器、闹钟、定时器 |
| v1.0 | 2026-08-27 | 基础功能：语音对话、WiFi 配网 |

---

## 📄 许可证

MIT License — 基于 Hermes Studio 开源协议

---

## 🔗 相关链接

- [Hermes Studio 官方](https://github.com/EKKOLearnAI/hermes-studio)
- [Hermes Agent](https://github.com/NousResearch/hermes-agent)
- [LilyGO T5 GitHub](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [GxEPD2 库](https://github.com/ZinggJM/GxEPD2)
- [PlatformIO](https://platformio.org/)
