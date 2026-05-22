#include "mqtt_handler.h"

// ============================================================
//  CONSTRUCTOR
// ============================================================
MQTTHandler::MQTTHandler(SecurityManager &sec)
  : _sec(sec),
    _client(_wifiClient),
    _last_reconnect_ms(0)
{
  // TLS: khong verify cert (de port sang)
  // Production: _wifiClient.setCACert(hivemq_root_ca)
  _wifiClient.setInsecure();
}

// ============================================================
//  BEGIN
// ============================================================
bool MQTTHandler::begin() {
  Serial.println("[MQTT] Initializing...");
  _client.setServer(MQTT_BROKER, MQTT_PORT);
  _client.setKeepAlive(MQTT_KEEPALIVE);
  _client.setBufferSize(1024);
  _client.setCallback(onMessageReceived);
  return reconnect();
}

// ============================================================
//  IS CONNECTED
// ============================================================
bool MQTTHandler::isConnected() {
  return _client.connected();
}

// ============================================================
//  RECONNECT — ket noi lai voi Last Will Testament
// ============================================================
bool MQTTHandler::reconnect() {
  // Tranh retry qua nhanh
  if (millis() - _last_reconnect_ms < 5000) return false;
  _last_reconnect_ms = millis();

  Serial.print("[MQTT] Connecting to broker...");

  // Last Will & Testament: tu dong publish "offline" khi mat ket noi
  String lwt_payload = "{\"device\":\"" + String(DEVICE_ID) + "\","
                       "\"status\":\"offline\"}";

  bool connected = _client.connect(
    MQTT_CLIENT_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    MQTT_TOPIC_STATUS,        // LWT topic
    MQTT_QOS,                 // LWT QoS
    true,                     // LWT retain
    lwt_payload.c_str()       // LWT message
  );

  if (!connected) {
    Serial.printf(" FAILED! State: %d\n", _client.state());
    return false;
  }

  Serial.println(" OK!");

  // Subscribe topic lenh dieu khien
  _client.subscribe(MQTT_TOPIC_COMMAND, MQTT_QOS);
  Serial.println("[MQTT] Subscribed to: " + String(MQTT_TOPIC_COMMAND));

  // Bao online ngay sau khi ket noi
  publishStatus("online");
  return true;
}

// ============================================================
//  PUBLISH SENSOR DATA (co chu ky HMAC)
// ============================================================
bool MQTTHandler::publishSensorData(const SensorData &data) {
  if (!isConnected()) {
    if (!reconnect()) return false;
  }

  String payload     = _buildSensorJSON(data);
  String signed_json = _sec.buildSignedJSON(payload);

  bool ok = _client.publish(
    MQTT_TOPIC_SENSOR,
    signed_json.c_str(),
    false   // retain = false cho sensor data
  );

  if (ok) {
    Serial.println("[MQTT] Sensor data published OK");
  } else {
    Serial.println("[MQTT] ERROR: Publish failed!");
  }
  return ok;
}

// ============================================================
//  PUBLISH STATUS
// ============================================================
bool MQTTHandler::publishStatus(const String &status) {
  String ts = _sec.getTimestampISO();
  String payload = "{\"device\":\"" + String(DEVICE_ID) + "\","
                   "\"status\":\"" + status + "\","
                   "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\","
                   "\"ts\":\"" + ts + "\"}";

  return _client.publish(
    MQTT_TOPIC_STATUS,
    payload.c_str(),
    true    // retain = true cho status
  );
}

// ============================================================
//  LOOP — bat buoc goi trong loop() chinh
// ============================================================
void MQTTHandler::loop() {
  if (!isConnected()) {
    Serial.println("[MQTT] Disconnected! Attempting reconnect...");
    reconnect();
  }
  _client.loop();
}

// ============================================================
//  CALLBACK — xu ly tin nhan nhan duoc
// ============================================================
void MQTTHandler::onMessageReceived(char* topic,
                                     byte* payload,
                                     unsigned int length)
{
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.printf("[MQTT] Message arrived [%s]: %s\n", topic, msg.c_str());

  // Parse JSON lenh dieu khien
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.println("[MQTT] ERROR: Invalid JSON command!");
    return;
  }

  // Xu ly lenh
  const char* cmd = doc["cmd"];
  if (!cmd) return;

  if (strcmp(cmd, "reboot") == 0) {
    Serial.println("[MQTT] CMD: Reboot received!");
    delay(1000);
    ESP.restart();

  } else if (strcmp(cmd, "status") == 0) {
    Serial.println("[MQTT] CMD: Status request.");
    // Status se duoc xu ly trong loop chinh

  } else {
    Serial.printf("[MQTT] CMD: Unknown command '%s'\n", cmd);
  }
}

// ============================================================
//  BUILD SENSOR JSON
// ============================================================
String MQTTHandler::_buildSensorJSON(const SensorData &data) {
  StaticJsonDocument<512> doc;

  doc["device_id"]    = DEVICE_ID;
  doc["location"]     = DEVICE_LOCATION;
  doc["timestamp_ms"] = data.timestamp_ms;

  JsonObject dht = doc.createNestedObject("dht11");
  dht["temp"]     = serialized(String(data.temperature, 1));
  dht["hum"]      = serialized(String(data.humidity, 1));

  JsonObject mq2  = doc.createNestedObject("mq2");
  mq2["raw"]      = data.gas_raw;
  mq2["ppm"]      = serialized(String(data.gas_ppm, 1));
  mq2["alert"]    = data.gas_alert;

  JsonObject ldr  = doc.createNestedObject("ldr");
  ldr["raw"]      = data.light_raw;
  ldr["pct"]      = data.light_percent;
  ldr["dark"]     = data.light_dark;

  String output;
  serializeJson(doc, output);
  return output;
}
