// Intégration Home Assistant par MQTT avec découverte automatique.
//
// Topics (base = settings.mqtt.baseTopic, ex. "mcz/mcz-a1b2c3") :
//   <base>/status        online / offline (LWT)
//   <base>/state         état JSON {"on":true,"power":3,"fan":"auto",...}
//   <base>/set           commande JSON (mêmes champs que l'API web)
//   <base>/set/switch    ON / OFF
//   <base>/set/power     1..5
//   <base>/set/fan       1..5 / auto
//   <base>/set/resend    (n'importe quoi) renvoie la dernière commande
//   <base>/temperature   température de la sonde DS18B20 en °C (si présente)
//   <base>/auto          mode autonome {"enabled":true,"target":20.5,"phase":"heat",...}
//   <base>/set/auto      ON / OFF : active / désactive le mode autonome
//
// Entités créées dans HA : interrupteur, puissance, ventilation, bouton
// "Renvoyer", signal WiFi, température (seulement si une sonde a été détectée),
// mode autonome (interrupteur), consigne et phase de régulation.
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

class Stove;
class TempSensor;

class HomeAssistant {
 public:
  HomeAssistant(Stove& stove, TempSensor& temp);

  void begin();
  void loop();
  void reconfigure();  // à appeler après modification des réglages MQTT

  bool connected() { return _mqtt.connected(); }
  const String& lastError() const { return _lastError; }

 private:
  bool connect();
  void onMessage(char* topic, uint8_t* payload, unsigned int len);
  void publishDiscovery();
  void publishState();
  void publishDiagnostics();
  void publishTemperature();
  void publishTemperatureDiscovery();
  void publishAuto();
  String topic(const char* suffix) const;

  Stove& _stove;
  TempSensor& _temp;
  WiFiClient _net;
  PubSubClient _mqtt;
  uint32_t _lastAttempt = 0;
  uint32_t _lastDiag = 0;
  bool _statePending = false;
  bool _tempPending = false;
  bool _autoPending = false;
  bool _tempDiscovered = false;
  String _lastError;
};
