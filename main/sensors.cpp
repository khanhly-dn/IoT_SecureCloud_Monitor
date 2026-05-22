#include "sensors.h"

// ============================================================
//  CONSTRUCTOR
// ============================================================
SensorsManager::SensorsManager()
  : _dht(PIN_DHT11, DHT_TYPE), _buf_idx(0)
{
  memset(_mq2_buf, 0, sizeof(_mq2_buf));
  memset(_ldr_buf, 0, sizeof(_ldr_buf));
}

// ============================================================
//  BEGIN
// ============================================================
bool SensorsManager::begin() {
  Serial.println("[SENSOR] Initializing...");

  // ADC 12-bit (0-4095), dai 0-3.3V
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // DHT11 can 2 giay on dinh
  _dht.begin();
  delay(2000);

  // Pre-fill moving average buffer
  for (int i = 0; i < FILTER_SIZE; i++) {
    _mq2_buf[i] = analogRead(PIN_MQ2_ANALOG);
    _ldr_buf[i] = analogRead(PIN_LDR_ANALOG);
    delay(20);
  }

  // Kiem tra DHT11
  float t, h;
  if (!_readDHT11(t, h)) {
    Serial.println("[SENSOR] WARNING: DHT11 may need warm-up.");
  } else {
    Serial.printf("[SENSOR] DHT11 OK — %.1f°C  %.1f%%\n", t, h);
  }

  Serial.println("[SENSOR] Ready.");
  return true;
}

// ============================================================
//  READ ALL
// ============================================================
SensorData SensorsManager::readAll() {
  SensorData d;
  memset(&d, 0, sizeof(d));
  d.timestamp_ms = millis();

  // DHT11
  d.dht_ok = _readDHT11(d.temperature, d.humidity);

  // MQ2
  d.gas_raw   = _readMQ2();
  d.gas_ppm   = _rawToPPM(d.gas_raw);
  d.gas_alert = (d.gas_raw > MQ2_THRESHOLD);

  // LDR Module — doc chan AO
  d.light_raw     = _readLDR();
  // AO cua module LDR: gia tri thap = sang, cao = toi
  d.light_percent = 100 - (int)((d.light_raw / 4095.0f) * 100.0f);
  d.light_percent = constrain(d.light_percent, 0, 100);
  d.light_dark    = (d.light_raw < LDR_THRESHOLD_DARK);

  d.valid = d.dht_ok;
  return d;
}

// ============================================================
//  DHT11
// ============================================================
bool SensorsManager::_readDHT11(float &temp, float &hum) {
  float t = _dht.readTemperature();
  float h = _dht.readHumidity();

  // Thu lai lan 2 neu loi
  if (isnan(t) || isnan(h)) {
    delay(500);
    t = _dht.readTemperature();
    h = _dht.readHumidity();
  }

  if (isnan(t) || isnan(h)) {
    Serial.println("[DHT11] ERROR: Read failed!");
    temp = 0.0f; hum = 0.0f;
    return false;
  }

  // Range check DHT11: -20~60°C, 0~100%
  if (t < -20.0f || t > 60.0f || h < 0.0f || h > 100.0f) {
    Serial.println("[DHT11] ERROR: Out of valid range!");
    return false;
  }

  temp = roundf(t * 10.0f) / 10.0f;
  hum  = roundf(h * 10.0f) / 10.0f;
  return true;
}

// ============================================================
//  MQ2 — doc analog co filter
// ============================================================
int SensorsManager::_readMQ2() {
  int raw = analogRead(PIN_MQ2_ANALOG);
  return _movingAvg(_mq2_buf, raw);
}

// ============================================================
//  MQ2 — quy doi sang PPM uoc tinh
//  Dua tren dac tinh MQ2 datasheet (LPG/Smoke curve)
// ============================================================
float SensorsManager::_rawToPPM(int raw) {
  if (raw <= 0) return 0.0f;
  // Tinh dien ap tren RL (VCC MQ2 = 5V)
  float v_rl    = (raw / 4095.0f) * 3.3f;   // ADC doc duoc (qua voltage divider)
  float v_total = 5.0f;
  // Rs = RL * (Vcc - Vrl) / Vrl  (RL = 10kOhm)
  float rs_ro   = (v_total - v_rl) / v_rl;
  if (rs_ro <= 0) return 0.0f;

  // PPM theo duong cong LPG cua MQ2 (log-log linear approximation)
  float ppm = 616.93f * pow(rs_ro, -2.475f);
  return constrain(ppm, 0.0f, 10000.0f);
}

// ============================================================
//  LDR — doc chan AO cua module, co filter
// ============================================================
int SensorsManager::_readLDR() {
  int raw = analogRead(PIN_LDR_ANALOG);
  return _movingAvg(_ldr_buf, raw);
}

// ============================================================
//  MOVING AVERAGE FILTER — giam nhieu ADC
// ============================================================
int SensorsManager::_movingAvg(int* buf, int new_val) {
  buf[_buf_idx % FILTER_SIZE] = new_val;
  _buf_idx++;
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += buf[i];
  return (int)(sum / FILTER_SIZE);
}

// ============================================================
//  PRINT — in du lieu ra Serial Monitor
// ============================================================
void SensorsManager::printData(const SensorData &d) {
  Serial.println("======================================");
  Serial.printf("[DHT11] Temp: %.1f°C  Hum: %.1f%%  OK:%d\n",
                d.temperature, d.humidity, d.dht_ok);
  Serial.printf("[MQ2]   Raw: %d  PPM: %.1f  ALERT:%d\n",
                d.gas_raw, d.gas_ppm, d.gas_alert);
  Serial.printf("[LDR]   Raw: %d  Light: %d%%  DARK:%d\n",
                d.light_raw, d.light_percent, d.light_dark);
  Serial.printf("[TIME]  %lu ms\n", d.timestamp_ms);
  Serial.println("======================================");
}
