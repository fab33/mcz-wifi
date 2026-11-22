// Mode autonome : le module allume, règle et éteint le poêle lui-même selon
// une programmation hebdomadaire et la température mesurée par la sonde
// DS18B20 (logique dans lib/Thermostat).
//
//  - La programmation et l'état actif/inactif sont enregistrés dans
//    /schedule.json (ils survivent aux redémarrages).
//  - Toute commande manuelle (page web, MQTT, bouton du module) qui modifie
//    l'état du poêle désactive le mode autonome.
//  - Sans heure (redémarrage sans accès au serveur de temps) : mode dégradé.
//    La régulation continue sur la dernière consigne connue (enregistrée dans
//    /schedule.json), sans jamais allumer le poêle : un poêle allumé est
//    éteint normalement à la consigne, puis reste éteint jusqu'au retour de
//    l'heure. Sans consigne connue, le mode autonome n'agit pas.
//  - Sans température, il n'allume pas le poêle (et l'éteint après 5 min).
//  - La ventilation est toujours laissée en automatique.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Thermostat.h>

#include <functional>
#include <vector>

class Stove;
class TempSensor;

class Autonomous {
 public:
  void begin(Stove& stove, TempSensor& temp);
  void loop();

  bool enabled() const { return _enabled; }
  void setEnabled(bool on);

  // Programmation : {"days":[[{"start":360,"end":480,"target":20.5}, ...], ...]}
  // (7 jours, lundi en premier ; minutes depuis minuit ; target null = arrêt).
  void scheduleToJson(JsonObject obj) const;
  bool scheduleFromJson(JsonObjectConst obj, String& error);

  // État courant pour l'interface, l'API et MQTT.
  void statusToJson(JsonObject obj);

  void onChange(std::function<void()> cb) { _listeners.push_back(cb); }

 private:
  void step();
  void apply();
  bool load();
  bool save() const;
  void notify();
  thermo::Params params() const;

  Stove* _stove = nullptr;
  TempSensor* _temp = nullptr;
  thermo::Week _week;
  thermo::Regulator _reg;
  bool _enabled = false;
  int16_t _target = thermo::TARGET_OFF;
  bool _timeValid = false;
  // Dernière consigne appliquée avec une heure valide (mode dégradé).
  bool _hasLastTarget = false;
  int16_t _lastTarget = thermo::TARGET_OFF;
  // Émettre l'état voulu à la prochaine régulation même s'il est déjà celui du
  // poêle : au démarrage du module et à l'activation, l'état mémorisé peut ne
  // plus correspondre au poêle, et c'est la première émission qui lance le
  // renvoi périodique (Stove::loop).
  bool _syncPending = true;
  uint32_t _lastStep = 0;
  std::vector<std::function<void()>> _listeners;
};

extern Autonomous autonomous;
