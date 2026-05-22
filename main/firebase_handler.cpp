#include "firebase_handler.h"

// ============================================================
//  CONSTRUCTOR
// ============================================================
FirebaseHandler::FirebaseHandler(SecurityManager &sec)
  : _sec(sec), _token(""), _token_issued_ms(0)
{
  // Khong verify certificate (de port sang moi mang)
  // Production: dung _wifiClient.setCACert(root_ca)
  _wifiClient.setInsecure();
}

// ============================================================
//  BEGIN — xac thuc lan dau
// ============================================================
bool FirebaseHandler::begin() {
  Serial.println("[FIREBASE] Authenticating...");
  return _authenticate();
}

// ============================================================
//  CHECK & REFRESH TOKEN (Firebase token song 1 gio)
// ============================================================
bool FirebaseHandler::checkAndRefreshToken() {
  if (_token.length() == 0 ||
      (millis() - _token_issued_ms) > TOKEN_REFRESH_MS) {
    Serial.println("[FIREBASE] Token expired, refreshing...");
    return _authenticate();
  }
  return true;
}

// ============================================================
//  SEND SENSOR DATA
//  Ghi vao 2 path:
//  1. /live   — du lieu hien tai (PATCH, ghi de)
//  2. /history — lich su (POST, them moi)
// ============================================================
bool FirebaseHandler::sendSensorData(const SensorData &data) {
  if (!checkAndRefreshToken()) return false;

  String payload = _buildSensorJSON(data);
  String signed_payload = _sec.buildSignedJSON(payload);

  // 1. Ghi live data (PATCH de chi update cac truong thay doi)
  bool live_ok = _httpPatch(String(FB_PATH_LIVE), signed_payload);

  // 2. Push vao history (POST tao node moi voi key tu dong)
  bool hist_ok = _httpPost(String(FB_PATH_HISTORY), signed_payload);

  if (live_ok) {
    Serial.println("[FIREBASE] Data sent OK");
  } else {
    Serial.println("[FIREBASE] ERROR: Send failed!");
  }

  return live_ok && hist_ok;
}

// ============================================================
//  UPDATE DEVICE STATUS
// ============================================================
bool FirebaseHandler::updateStatus(const String &status) {
  if (_token.length() == 0) return false;

  String ts = _sec.getTimestampISO();
  String json = "{\"status\":\"" + status + "\","
                "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\","
                "\"location\":\"" + String(DEVICE_LOCATION) + "\","
                "\"last_seen\":\"" + ts + "\"}";

  return _httpPatch(String(FB_PATH_STATUS), json);
}

// ============================================================
//  AUTHENTICATE — lay ID Token tu Firebase Auth REST API
// ============================================================
bool FirebaseHandler::_authenticate() {
  HTTPClient http;
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
  url += String(FIREBASE_API_KEY);

  String body = "{\"email\":\"" + String(FIREBASE_EMAIL) + "\","
                "\"password\":\"" + String(FIREBASE_PASSWORD) + "\","
                "\"returnSecureToken\":true}";

  http.begin(_wifiClient, url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);

  if (code != 200) {
    Serial.printf("[FIREBASE] Auth failed! HTTP %d\n", code);
    Serial.println(http.getString());
    http.end();
    return false;
  }

  // Parse JSON de lay idToken
  String resp = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.println("[FIREBASE] JSON parse error on auth!");
    return false;
  }

  _token = doc["idToken"].as<String>();
  _token_issued_ms = millis();
  Serial.println("[FIREBASE] Auth OK. Token received.");
  return (_token.length() > 0);
}

// ============================================================
//  HTTP PATCH — cap nhat du lieu (ghi de)
// ============================================================
bool FirebaseHandler::_httpPatch(const String &path, const String &json_body) {
  HTTPClient http;
  String url = "https://" + String(FIREBASE_HOST) + path + ".json?auth=" + _token;

  http.begin(_wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-ID", DEVICE_ID);

  // PATCH khong co trong HTTPClient mac dinh -> dung sendRequest
  int code = http.sendRequest("PATCH",
               (uint8_t*)json_body.c_str(), json_body.length());

  bool ok = (code == 200);
  if (!ok) Serial.printf("[FIREBASE] PATCH failed! HTTP %d\n", code);
  http.end();
  return ok;
}

// ============================================================
//  HTTP POST — push them node moi vao history
// ============================================================
bool FirebaseHandler::_httpPost(const String &path, const String &json_body) {
  HTTPClient http;
  String url = "https://" + String(FIREBASE_HOST) + path + ".json?auth=" + _token;

  http.begin(_wifiClient, url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST((uint8_t*)json_body.c_str(), json_body.length());

  bool ok = (code == 200 || code == 201);
  if (!ok) Serial.printf("[FIREBASE] POST failed! HTTP %d\n", code);
  http.end();
  return ok;
}

// ============================================================
//  BUILD SENSOR JSON
// ============================================================
String FirebaseHandler::_buildSensorJSON(const SensorData &data) {
  StaticJsonDocument<512> doc;

  doc["device_id"]      = DEVICE_ID;
  doc["location"]       = DEVICE_LOCATION;
  doc["timestamp_ms"]   = data.timestamp_ms;
  doc["timestamp"]      = _sec.getTimestampISO();

  JsonObject dht = doc.createNestedObject("dht11");
  dht["temperature"]    = serialized(String(data.temperature, 1));
  dht["humidity"]       = serialized(String(data.humidity, 1));
  dht["ok"]             = data.dht_ok;

  JsonObject mq2 = doc.createNestedObject("mq2");
  mq2["raw"]            = data.gas_raw;
  mq2["ppm"]            = serialized(String(data.gas_ppm, 1));
  mq2["alert"]          = data.gas_alert;

  JsonObject ldr = doc.createNestedObject("ldr");
  ldr["raw"]            = data.light_raw;
  ldr["percent"]        = data.light_percent;
  ldr["dark"]           = data.light_dark;

  String output;
  serializeJson(doc, output);
  return output;
}
