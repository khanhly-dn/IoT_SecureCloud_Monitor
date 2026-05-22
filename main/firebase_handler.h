#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "security.h"

// ============================================================
//  CLASS FIREBASE HANDLER
//  - Xac thuc bang Email/Password (REST API)
//  - Ghi du lieu live va history
//  - TLS/SSL qua WiFiClientSecure
// ============================================================
class FirebaseHandler {
public:
  FirebaseHandler(SecurityManager &sec);

  // Khoi tao, dang nhap lay ID token
  bool begin();

  // Kiem tra va refresh token neu gan het han
  bool checkAndRefreshToken();

  // Ghi du lieu cam bien len Firebase
  bool sendSensorData(const SensorData &data);

  // Cap nhat trang thai device (online/offline)
  bool updateStatus(const String &status);

  bool isConnected() const { return _token.length() > 0; }

private:
  SecurityManager &_sec;
  String           _token;
  unsigned long    _token_issued_ms;

  // REST API calls
  bool   _authenticate();
  bool   _httpPatch(const String &path, const String &json_body);
  bool   _httpPost(const String &path, const String &json_body);
  String _httpGet(const String &path);

  // Tao JSON tu SensorData
  String _buildSensorJSON(const SensorData &data);

  WiFiClientSecure _wifiClient;
};

#endif // FIREBASE_HANDLER_H
