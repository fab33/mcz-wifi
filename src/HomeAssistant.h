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
//
// Entités créées dans HA : interrupteur, puissance, ventilation, bouton
// "Renvoyer", signal WiFi.
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

class Stove;

class HomeAssistant {
 public:
  explicit HomeAssistant(Stove& stove);

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
  String topic(const char* suffix) const;

  Stove& _stove;
  WiFiClient _net;
  PubSubClient _mqtt;
  uint32_t _lastAttempt = 0;
  uint32_t _lastDiag = 0;
  bool _statePending = false;
  String _lastError;
};
