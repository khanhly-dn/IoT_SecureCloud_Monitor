#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include <mbedtls/md.h>
#include "config.h"

// ============================================================
//  CLASS SECURITY MANAGER
//  - HMAC-SHA256 ky du lieu truoc khi gui
//  - Token management cho Firebase
//  - Kiem tra data integrity
// ============================================================
class SecurityManager {
public:
  SecurityManager();

  // HMAC-SHA256: ky chuoi payload, tra ve hex string
  String signPayload(const String &payload);

  // Tao JSON co chu ky: {"data":{...},"sig":"abcdef..."}
  String buildSignedJSON(const String &json_payload);

  // Kiem tra chu ky nhan vao (dung cho subscribe MQTT)
  bool verifySignature(const String &payload, const String &expected_sig);

  // Tao timestamp string dang ISO8601 (dung time tu NTP)
  String getTimestampISO();

  // Base64 encode (dung cho HTTP header neu can)
  String base64Encode(const uint8_t* data, size_t len);

private:
  // HMAC-SHA256 internal
  bool _hmacSHA256(const String &message,
                   const String &key,
                   uint8_t* out_hash,
                   size_t  &out_len);

  String _bytesToHex(const uint8_t* data, size_t len);
};

#endif // SECURITY_H
