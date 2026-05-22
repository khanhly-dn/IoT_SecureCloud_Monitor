# 🔐 IoT SecureCloud Monitor

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-003AFF?style=for-the-badge&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Sensor-DHT11%20%7C%20MQ2%20%7C%20LDR-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Cloud-Firebase%20%7C%20MQTT-FFCA28?style=for-the-badge&logo=firebase&logoColor=black" />
  <img src="https://img.shields.io/badge/Security-HMAC--SHA256%20%7C%20TLS-red?style=for-the-badge&logo=letsencrypt&logoColor=white" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" />
</p>

<p align="center">
  Hệ thống giám sát môi trường IoT theo thời gian thực sử dụng <strong>ESP32</strong>, tích hợp bảo mật nhiều lớp với <strong>HMAC-SHA256</strong>, <strong>TLS/SSL</strong> và đẩy dữ liệu đồng thời lên <strong>Firebase Realtime Database</strong> và <strong>MQTT Broker</strong>.
</p>

---

## 📌 Giới thiệu

**IoT SecureCloud Monitor** là dự án xây dựng hệ thống thu thập và truyền dữ liệu cảm biến môi trường lên cloud một cách **bảo mật và đáng tin cậy**.

Điểm nổi bật của dự án so với các hệ thống IoT thông thường:

- 🔒 **Bảo mật dữ liệu thực sự** — mỗi gói tin được ký bằng HMAC-SHA256 trước khi gửi, đảm bảo tính toàn vẹn và chống giả mạo
- ☁️ **Dual-cloud architecture** — gửi dữ liệu đồng thời lên Firebase (lưu trữ lịch sử) và MQTT Broker (streaming thời gian thực)
- 🔐 **TLS/SSL end-to-end** — toàn bộ kết nối cloud mã hóa, không có dữ liệu nào truyền dạng plaintext
- 🛡️ **Production-grade code structure** — tách module rõ ràng, watchdog timer, auto-reconnect, token refresh tự động

---

## 🎯 Chức năng chính

- Đọc liên tục **nhiệt độ, độ ẩm** (DHT11), **nồng độ khí gas/khói** (MQ2), **cường độ ánh sáng** (LDR Module)
- **Ký số payload** bằng HMAC-SHA256 trước mỗi lần gửi lên cloud
- **Xác thực Firebase** bằng Email/Password Auth — tự động refresh token mỗi 55 phút
- **Kết nối MQTT over TLS** port 8883 với Last Will Testament (tự động báo offline khi mất kết nối)
- **Moving average filter** giảm nhiễu ADC trên MQ2 và LDR
- **Watchdog timer 30 giây** — tự động reset ESP32 nếu bị treo
- **Cảnh báo thời gian thực** khi phát hiện khí gas hoặc ánh sáng bất thường
- Lưu **lịch sử dữ liệu** vào Firebase và **stream real-time** qua MQTT

---

## 🧩 Sơ đồ hoạt động

<p align="center">
  <img width="750" alt="Sơ đồ hoạt động" src="https://github.com/khanhly-dn/IoT_SecureCloud_Monitor/blob/main/SDHD.png?raw=true" />
</p>

```
┌─────────────┐     ┌─────────────┐     ┌──────────────┐
│   DHT11     │     │    MQ2      │     │  LDR Module  │
│  GPIO4·3.3V │     │ GPIO34·5V  │     │  GPIO35·3.3V │
└──────┬──────┘     └──────┬──────┘     └──────┬───────┘
       │                   │                    │
       └───────────────────┼────────────────────┘
                           │ sensors.cpp (moving avg filter)
                    ┌──────▼──────┐
                    │   ESP32     │
                    │  240MHz     │
                    │  WiFi WPA2  │
                    └──────┬──────┘
                           │ security.cpp
                           │ HMAC-SHA256 sign payload
                           │ {"data":{...},"sig":"a3f9...","ts":"..."}
              ┌────────────┴────────────┐
              │                         │
    ┌─────────▼──────────┐   ┌──────────▼─────────┐
    │  Firebase RTDB     │   │   HiveMQ Cloud     │
    │  Auth Token (1h)   │   │   TLS port 8883    │
    │  /live  → PATCH    │   │   QoS 1 · LWT      │
    │  /history → POST   │   │   Signed payload   │
    └─────────┬──────────┘   └──────────┬─────────┘
              │                         │
    ┌─────────▼──────────┐   ┌──────────▼─────────┐
    │  Firebase Console  │   │  MQTT Web Client   │
    │  Real-time viewer  │   │  Subscribe topics  │
    └────────────────────┘   └────────────────────┘
```

---

## 🛠️ Phần cứng sử dụng

| Linh kiện | Kết nối ESP32 | Mô tả |
|---|---|---|
| **ESP32 DevKit v1** | — | Vi điều khiển chính, WiFi tích hợp |
| **DHT11** | DATA → GPIO4, VCC → 3.3V | Đo nhiệt độ (°C) và độ ẩm (%) |
| **MQ2** | AOUT → GPIO34, VCC → **5V** | Phát hiện khói, gas, LPG (analog) |
| **LDR Module** | AO → GPIO35, VCC → 3.3V | Đo cường độ ánh sáng (%) |
| **Điện trở 10kΩ** | DATA–VCC (DHT11) | Pull-up cho chân DATA |

> ⚠️ **Lưu ý:** MQ2 bắt buộc dùng nguồn **5V (VIN)**, không dùng 3.3V — sensor sẽ không hoạt động đúng. GPIO34 và GPIO35 là **Input-Only**, phù hợp để đọc tín hiệu analog.

<p align="center">
  <img width="650" alt="Sơ đồ thiết bị" src="https://github.com/khanhly-dn/IoT_SecureCloud_Monitor/blob/main/TB.png?raw=true" />
</p>

---

## 💻 Công nghệ & Thư viện

**Ngôn ngữ:** Arduino C++ (ESP32 core)

**Thư viện cần cài:**

| Thư viện | Tác giả | Chức năng |
|---|---|---|
| `DHT sensor library` | Adafruit | Đọc DHT11 |
| `Adafruit Unified Sensor` | Adafruit | Dependency của DHT |
| `ArduinoJson` | Benoit Blanchon | Tạo và parse JSON |
| `PubSubClient` | Nick O'Leary | MQTT client |

**Thư viện built-in ESP32 (không cần cài thêm):**
- `mbedtls` — HMAC-SHA256, Base64
- `WiFiClientSecure` — TLS/SSL
- `HTTPClient` — Firebase REST API
- `esp_task_wdt` — Watchdog timer

---

## 🔐 Kiến trúc bảo mật

```
Lớp 1 — WiFi WPA2          : Mã hóa kết nối mạng nội bộ
Lớp 2 — Firebase Auth Token : Xác thực thiết bị với Firebase (JWT, TTL 1h)
Lớp 3 — MQTT Credentials   : Username/Password cho HiveMQ Cloud
Lớp 4 — TLS/SSL port 8883  : Mã hóa toàn bộ luồng MQTT
Lớp 5 — HMAC-SHA256        : Ký số từng payload, chống giả mạo và tamper
Lớp 6 — Timestamp + DeviceID : Chống replay attack
```

Mỗi gói tin gửi lên có dạng:
```json
{
  "data": {
    "device_id": "esp32_01",
    "dht11": { "temperature": 32.3, "humidity": 76.0 },
    "mq2":  { "raw": 120, "ppm": 0.0, "alert": false },
    "ldr":  { "raw": 3692, "percent": 10, "dark": false }
  },
  "sig": "a3f97c8b2e1d...",
  "ts":  "2026-05-22T15:45:58Z",
  "device": "esp32_01"
}
```

---

## 📁 Cấu trúc dự án

```
IoT_SecureCloud_Monitor/
├── main/
│   ├── main.ino                 ← File chính, nạp vào ESP32
│   ├── config.example.h         ← Template cấu hình (xem bên dưới)
│   ├── sensors.h / sensors.cpp  ← Đọc DHT11, MQ2, LDR + filter
│   ├── security.h / security.cpp← HMAC-SHA256, timestamp, Base64
│   ├── firebase_handler.h/.cpp  ← Auth token, gửi live + history
│   ├── mqtt_handler.h/.cpp      ← TLS, publish, subscribe, LWT
│   └── .gitignore
└── README.md
```

> 🔴 **`config.h` không được đăng lên Git** vì chứa thông tin nhạy cảm gồm mật khẩu WiFi, Firebase API Key, Firebase Email/Password và MQTT Username/Password. File `.gitignore` đã được cấu hình để tự động loại trừ `config.h`. Thay vào đó, dùng `config.example.h` làm template.

---

## 🚀 Hướng dẫn cài đặt

**Bước 1 — Cài Arduino IDE và ESP32 board**
```
Thêm vào Board Manager URL:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Vào Tools → Board → Boards Manager → tìm "esp32" → Install
```

**Bước 2 — Cài thư viện** (Tools → Manage Libraries)
```
DHT sensor library     (Adafruit)
Adafruit Unified Sensor (Adafruit)
ArduinoJson            (Benoit Blanchon)
PubSubClient           (Nick O'Leary)
```

**Bước 3 — Tạo file cấu hình**
```bash
# Sao chép file mẫu
cp main/config.example.h main/config.h

# Mở config.h và điền thông tin thật của bạn
```

```cpp
// config.h — KHÔNG commit file này lên Git!
#define WIFI_SSID        "Ten_WiFi_cua_ban"
#define WIFI_PASSWORD    "Mat_khau_WiFi"

#define FIREBASE_HOST    "your-project-id-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY "AIzaSy..."
#define FIREBASE_EMAIL   "your-email@gmail.com"
#define FIREBASE_PASSWORD "your-password"

#define MQTT_BROKER      "xxxx.s1.eu.hivemq.cloud"
#define MQTT_USERNAME    "your-mqtt-username"
#define MQTT_PASSWORD    "your-mqtt-password"

#define HMAC_SECRET_KEY  "your-32-byte-secret-key-here!!!"
```

**Bước 4 — Tạo Firebase và MQTT Broker**

| Dịch vụ | Link | Gói miễn phí |
|---|---|---|
| Firebase Realtime Database | https://console.firebase.google.com | Spark plan (free) |
| HiveMQ Cloud MQTT | https://www.hivemq.com/mqtt-cloud-broker | Serverless (free, 100 connections) |

**Bước 5 — Nạp code**
```
1. Mở file main/main.ino trong Arduino IDE
2. Tools → Board → ESP32 Dev Module
3. Tools → Port → chọn đúng COM port
4. Nhấn Upload (→)
5. Mở Serial Monitor (115200 baud) theo dõi kết nối
```

---

## 📊 Thông số hệ thống

| Thông số | Giá trị |
|---|---|
| Chu kỳ đọc cảm biến | 5 giây |
| Chu kỳ gửi MQTT | 5 giây |
| Chu kỳ gửi Firebase | 10 giây |
| Watchdog timeout | 30 giây |
| Firebase token TTL | 1 giờ (auto-refresh lúc 55 phút) |
| Moving average window | 5 mẫu |
| Ngưỡng cảnh báo MQ2 | ADC > 1500 |
| Ngưỡng cảnh báo LDR | ADC < 500 (tối) |
| MQTT QoS | 1 (at least once) |
| MQTT port | 8883 (TLS) |

---

## 📷 Demo

<p align="center">
  <img width="700" alt="Demo sản phẩm" src="https://github.com/khanhly-dn/IoT_SecureCloud_Monitor/blob/main/SP.jpg?raw=true" />
</p>

---

## 🔭 Hướng phát triển

- [ ] Dashboard web hiển thị biểu đồ dữ liệu theo thời gian thực
- [ ] Tích hợp cảnh báo qua Telegram Bot
- [ ] OTA firmware update qua WiFi
- [ ] Hỗ trợ nhiều thiết bị ESP32 trên cùng hệ thống
- [ ] Thêm certificate pinning cho TLS
- [ ] Node-RED dashboard integration

---

## 👤 Thực hiện

**Lý Gia Khánh**  
Khoa Công nghệ Thông tin – Trường Đại học Đại Nam

---

<p align="center">
  Using ESP32 · Firebase · HiveMQ · HMAC-SHA256 · Arduino
</p>
