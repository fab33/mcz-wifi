// Sonde de température DS18B20 optionnelle (1-Wire sur D2 / GPIO4, broche SDA
// du connecteur I2C du shield).
//
// Détection automatique : si aucune sonde n'est branchée, rien n'est publié
// (une nouvelle recherche a lieu toutes les minutes). La conversion est lancée
// sans attente pour ne pas bloquer le WiFi : la valeur est lue ~750 ms plus tard.
#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#include <functional>

class TempSensor {
 public:
  TempSensor();
  void begin();
  void loop();

  bool present() const { return _present; }
  bool valid() const { return _valid; }
  float celsius() const { return _celsius; }  // correction de réglage incluse
  uint32_t ageMs() const { return millis() - _lastReading; }

  // Appelé à chaque nouvelle mesure valide, ou quand la sonde apparaît / disparaît.
  void onChange(std::function<void()> cb) { _onChange = cb; }

 private:
  bool search();
  void notify();

  OneWire _wire;
  DallasTemperature _dallas;
  DeviceAddress _addr;
  bool _present = false;
  bool _valid = false;
  bool _converting = false;
  float _celsius = NAN;
  uint8_t _failures = 0;
  uint32_t _lastSearch = 0;
  uint32_t _lastRequest = 0;
  uint32_t _lastReading = 0;
  std::function<void()> _onChange;
};
