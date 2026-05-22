// ============================================================
//  IoT SECURE CLOUD DATA UPLOAD
//  Repo   : iot-cloud-data-upload
//  Board  : ESP32 DevKit v1
//  Sensors: DHT11 (GPIO4) | MQ2 (GPIO34) | LDR Module (GPIO35)
//  Cloud  : Firebase Realtime DB + MQTT over TLS
//  Security: HMAC-SHA256 payload signing, Firebase Auth Token
//
//  Thu vien can cai trong Arduino IDE / PlatformIO:
//    - DHT sensor library (Adafruit)
//    - Adafruit Unified Sensor
//    - ArduinoJson (Benoit Blanchon)
//    - PubSubClient (Nick O'Leary)
//    - Firebase ESP Client (Mobizt) [tuy chon]
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config.h"
#include "sensors.h"
#include "security.h"
#include "firebase_handler.h"
#include "mqtt_handler.h"

// ============================================================
//  OBJECTS KHOI TAO
// ============================================================
SensorsManager  sensors;
SecurityManager security;
FirebaseHandler firebase(security);
MQTTHandler     mqtt(security);

// ============================================================
//  TIMER VARIABLES
// ============================================================
unsigned long last_sensor_read_ms   = 0;
unsigned long last_firebase_send_ms = 0;
unsigned long last_mqtt_send_ms     = 0;
unsigned long last_status_ms        = 0;

// Du lieu cam bien moi nhat
SensorData currentData;
bool       dataReady = false;

// ============================================================
//  WIFI CONNECT
// ============================================================
bool connectWiFi() {
  Serial.printf("\n[WIFI] Connecting to: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < WIFI_RETRY_COUNT * 10) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] FAILED!");
    return false;
  }

  Serial.println("\n[WIFI] Connected!");
  Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
  return true;
}

// ============================================================
//  NTP TIME SYNC
// ============================================================
void syncNTPTime() {
  Serial.println("[NTP] Syncing time...");
  // GMT+7 cho Viet Nam
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    delay(500);
    retry++;
    Serial.print(".");
  }

  if (retry < 10) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("\n[NTP] Time synced: %s\n", buf);
  } else {
    Serial.println("\n[NTP] WARNING: Time sync failed!");
  }
}

// ============================================================
//  WIFI MONITOR — tu dong ket noi lai
// ============================================================
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection! Reconnecting...");
    WiFi.disconnect();
    delay(1000);
    connectWiFi();
  }
}

// ============================================================
//  PRINT SYSTEM INFO
// ============================================================
void printSystemInfo() {
  Serial.println("\n========================================");
  Serial.println("  IoT Secure Cloud Upload — v" FIRMWARE_VERSION);
  Serial.println("========================================");
  Serial.printf("  Device ID  : %s\n", DEVICE_ID);
  Serial.printf("  Location   : %s\n", DEVICE_LOCATION);
  Serial.printf("  Free Heap  : %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  CPU Freq   : %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("  Flash Size : %d bytes\n", ESP.getFlashChipSize());
  Serial.println("========================================\n");
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  printSystemInfo();

  // 1. Watchdog Timer — tu dong reset neu bi treo 30 giay
  esp_task_wdt_config_t wdt_config = {
  .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
  .idle_core_mask = (1 << 0),
  .trigger_panic = true
};
esp_task_wdt_reconfigure(&wdt_config);
esp_task_wdt_add(NULL);

  // 2. Ket noi WiFi
  if (!connectWiFi()) {
    Serial.println("[SYSTEM] CRITICAL: No WiFi! Rebooting...");
    delay(3000);
    ESP.restart();
  }

  // 3. Dong bo thoi gian NTP
  syncNTPTime();

  // 4. Khoi tao cam bien
  sensors.begin();

  // 5. Ket noi Firebase
  if (!firebase.begin()) {
    Serial.println("[SYSTEM] WARNING: Firebase auth failed! Retrying in loop...");
  }

  // 6. Ket noi MQTT
  if (!mqtt.begin()) {
    Serial.println("[SYSTEM] WARNING: MQTT connect failed! Retrying in loop...");
  }

  // 7. Bao trang thai online
  firebase.updateStatus("online");

  Serial.println("\n[SYSTEM] Setup complete. Entering main loop...\n");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();

  unsigned long now = millis();

  // --- 1. Kiem tra WiFi ---
  checkWiFiConnection();

  // --- 2. Doc cam bien ---
  if (now - last_sensor_read_ms >= SENSOR_READ_INTERVAL_MS) {
    last_sensor_read_ms = now;
    currentData = sensors.readAll();
    dataReady   = currentData.valid;

    // In du lieu ra Serial Monitor
    if (dataReady) {
      sensors.printData(currentData);

      // Canh bao khi co su co
      if (currentData.gas_alert) {
        Serial.println("!!! ALERT: GAS/SMOKE DETECTED !!!");
      }
      if (currentData.light_dark) {
        Serial.println("!!! ALERT: LIGHT TOO LOW !!!");
      }
    } else {
      Serial.println("[LOOP] WARNING: Sensor data invalid, skipping send.");
    }
  }

  // --- 3. Gui MQTT ---
  if (dataReady && (now - last_mqtt_send_ms >= MQTT_SEND_INTERVAL_MS)) {
    last_mqtt_send_ms = now;
    mqtt.publishSensorData(currentData);
  }

  // --- 4. Gui Firebase ---
  if (dataReady && (now - last_firebase_send_ms >= FIREBASE_SEND_INTERVAL_MS)) {
    last_firebase_send_ms = now;
    firebase.sendSensorData(currentData);
  }

  // --- 5. Giu ket noi MQTT ---
  mqtt.loop();

  // --- 6. Refresh token Firebase dinh ky ---
  firebase.checkAndRefreshToken();

  // --- 7. Cap nhat status moi 5 phut ---
  if (now - last_status_ms >= 300000UL) {
    last_status_ms = now;
    String status_info = "online|heap:" + String(ESP.getFreeHeap())
                       + "|rssi:" + String(WiFi.RSSI());
    firebase.updateStatus(status_info);
    mqtt.publishStatus("online");
  }

  delay(100);
}
