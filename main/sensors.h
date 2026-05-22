#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <DHT.h>
#include "config.h"

// ============================================================
//  STRUCT DU LIEU CAM BIEN
// ============================================================
struct SensorData {
  // DHT11
  float         temperature;       // Nhiet do (°C)
  float         humidity;          // Do am (%)
  bool          dht_ok;

  // MQ2
  int           gas_raw;           // ADC 0-4095
  float         gas_ppm;           // PPM uoc tinh
  bool          gas_alert;         // Canh bao khi vuot nguong

  // LDR Module
  int           light_raw;         // ADC 0-4095
  int           light_percent;     // 0=toi, 100=sang
  bool          light_dark;        // Canh bao khi qua toi

  // Metadata
  unsigned long timestamp_ms;
  bool          valid;
};

// ============================================================
//  CLASS SENSORS MANAGER
// ============================================================
class SensorsManager {
public:
  SensorsManager();
  bool       begin();
  SensorData readAll();
  void       printData(const SensorData &d);

private:
  DHT _dht;

  // Moving average filter giam nhieu ADC
  static const int FILTER_SIZE = 5;
  int _mq2_buf[FILTER_SIZE];
  int _ldr_buf[FILTER_SIZE];
  int _buf_idx;

  bool  _readDHT11(float &temp, float &hum);
  int   _readMQ2();
  int   _readLDR();
  float _rawToPPM(int raw);
  int   _movingAvg(int* buf, int new_val);
};

#endif // SENSORS_H
