#include "security.h"
#include <mbedtls/base64.h>
#include <time.h>

// ============================================================
//  CONSTRUCTOR
// ============================================================
SecurityManager::SecurityManager() {}

// ============================================================
//  HMAC-SHA256 — ky payload voi secret key
//  Tra ve chuoi hex 64 ky tu
// ============================================================
String SecurityManager::signPayload(const String &payload) {
  uint8_t hash[32];
  size_t  hash_len = 0;

  if (!_hmacSHA256(payload, String(HMAC_SECRET_KEY), hash, hash_len)) {
    Serial.println("[SECURITY] ERROR: HMAC signing failed!");
    return "";
  }

  return _bytesToHex(hash, hash_len);
}

// ============================================================
//  BUILD SIGNED JSON
//  Input:  {"temperature":28.5,"humidity":65.0,...}
//  Output: {"data":{"temperature":28.5,...},"sig":"a3f9...","ts":"2024-..."}
// ============================================================
String SecurityManager::buildSignedJSON(const String &json_payload) {
  String signature = signPayload(json_payload);
  String timestamp = getTimestampISO();

  String result = "{";
  result += "\"data\":" + json_payload + ",";
  result += "\"sig\":\"" + signature + "\",";
  result += "\"ts\":\"" + timestamp + "\",";
  result += "\"device\":\"" + String(DEVICE_ID) + "\"";
  result += "}";

  return result;
}

// ============================================================
//  VERIFY SIGNATURE — xac thuc chu ky nhan duoc
// ============================================================
bool SecurityManager::verifySignature(const String &payload, const String &expected_sig) {
  String computed = signPayload(payload);
  if (computed.length() == 0) return false;

  // So sanh constant-time de chong timing attack
  if (computed.length() != expected_sig.length()) return false;

  uint8_t diff = 0;
  for (size_t i = 0; i < computed.length(); i++) {
    diff |= (uint8_t)(computed[i] ^ expected_sig[i]);
  }
  return (diff == 0);
}

// ============================================================
//  TIMESTAMP ISO8601 — lay thoi gian tu NTP
//  Format: 2024-01-15T08:30:00Z
// ============================================================
String SecurityManager::getTimestampISO() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // NTP chua san sang, tra ve millis thay the
    return "millis:" + String(millis());
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

// ============================================================
//  BASE64 ENCODE
// ============================================================
String SecurityManager::base64Encode(const uint8_t* data, size_t len) {
  size_t out_len = 0;
  // Tinh kich thuoc output can thiet
  mbedtls_base64_encode(nullptr, 0, &out_len, data, len);

  uint8_t* buf = new uint8_t[out_len + 1];
  mbedtls_base64_encode(buf, out_len + 1, &out_len, data, len);
  buf[out_len] = '\0';

  String result = String((char*)buf);
  delete[] buf;
  return result;
}

// ============================================================
//  HMAC-SHA256 INTERNAL (dung mbedTLS co san tren ESP32)
// ============================================================
bool SecurityManager::_hmacSHA256(const String &message,
                                   const String &key,
                                   uint8_t* out_hash,
                                   size_t  &out_len)
{
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&ctx);

  if (mbedtls_md_setup(&ctx, info, 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_starts(&ctx,
        (const uint8_t*)key.c_str(), key.length()) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_update(&ctx,
        (const uint8_t*)message.c_str(), message.length()) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  if (mbedtls_md_hmac_finish(&ctx, out_hash) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  mbedtls_md_free(&ctx);
  out_len = 32; // SHA256 luon cho ra 32 bytes
  return true;
}

// ============================================================
//  BYTES TO HEX STRING
// ============================================================
String SecurityManager::_bytesToHex(const uint8_t* data, size_t len) {
  String hex = "";
  hex.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) hex += "0";
    hex += String(data[i], HEX);
  }
  return hex;
}
