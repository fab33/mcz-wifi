#include "TempSensor.h"

#include "Settings.h"
#include "config.h"

static const uint32_t SEARCH_INTERVAL_MS = 60000;   // recherche si aucune sonde
static const uint32_t CONVERSION_MS = 750;          // résolution 12 bits
static const uint8_t MAX_FAILURES = 3;              // lectures ratées avant "sonde absente"

TempSensor::TempSensor() : _wire(PIN_TEMP), _dallas(&_wire) {}

void TempSensor::begin() {
  _dallas.begin();
  _dallas.setWaitForConversion(false);
  search();
}

bool TempSensor::search() {
  _lastSearch = millis();
  _wire.reset_search();
  bool found = false;
  while (_wire.search(_addr)) {
    if (OneWire::crc8(_addr, 7) == _addr[7] && _addr[0] == DS18B20MODEL) {
      found = true;
      break;
    }
  }
  if (found) {
    _dallas.setResolution(_addr, 12);
    _failures = 0;
    _converting = false;
    _lastRequest = 0;
    Serial.println("[Temp] Sonde DS18B20 détectée");
  }
  if (found != _present) {
    _present = found;
    if (!found) _valid = false;
    notify();
  }
  return found;
}

void TempSensor::loop() {
  if (!_present) {
    if (millis() - _lastSearch > SEARCH_INTERVAL_MS) search();
    return;
  }
  const uint32_t now = millis();
  if (!_converting) {
    if (_lastRequest == 0 || now - _lastRequest >= TEMP_READ_INTERVAL_MS) {
      _dallas.requestTemperaturesByAddress(_addr);
      _lastRequest = now;
      _converting = true;
    }
    return;
  }
  if (now - _lastRequest < CONVERSION_MS) return;
  _converting = false;

  float t = _dallas.getTempC(_addr);
  // -127 : sonde déconnectée ; 85 : valeur de mise sous tension (conversion non faite)
  if (t == DEVICE_DISCONNECTED_C || t == 85.0f) {
    if (++_failures >= MAX_FAILURES) {
      Serial.println("[Temp] Sonde perdue");
      _present = _valid = false;
      _lastSearch = now;
      notify();
    }
    return;
  }
  _failures = 0;
  _celsius = roundf((t + settings.sensor.offset) * 10.0f) / 10.0f;
  _valid = true;
  _lastReading = now;
  notify();
}

void TempSensor::notify() {
  if (_onChange) _onChange();
}
