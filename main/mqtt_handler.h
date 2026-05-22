#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "security.h"

// ============================================================
//  CLASS MQTT HANDLER
//  - Ket noi TLS/SSL port 8883
//  - Publish sensor data voi chu ky HMAC
//  - Subscribe lenh dieu khien tu server
//  - Auto-reconnect
// ============================================================
class MQTTHandler {
public:
  MQTTHandler(SecurityManager &sec);

  bool begin();
  bool isConnected();
  bool reconnect();

  // Publish du lieu cam bien
  bool publishSensorData(const SensorData &data);

  // Publish trang thai device
  bool publishStatus(const String &status);

  // Goi trong loop() de xu ly tin nhan den
  void loop();

  // Callback xu ly lenh tu server
  static void onMessageReceived(char* topic, byte* payload, unsigned int length);

private:
  SecurityManager  &_sec;
  WiFiClientSecure  _wifiClient;
  PubSubClient      _client;

  unsigned long _last_reconnect_ms;

  bool   _connect();
  String _buildSensorJSON(const SensorData &data);

  // Last Will & Testament — tu dong bao offline khi mat ket noi
  void   _setupLWT();
};

#endif // MQTT_HANDLER_H
