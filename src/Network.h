// Gestion du WiFi :
//  - connexion au réseau mémorisé ;
//  - sans réseau configuré ou en cas d'échec : point d'accès "MCZ-xxxxxx" avec
//    portail captif pour saisir les identifiants (nouvel essai de connexion
//    toutes les minutes tant que le portail est ouvert) ;
//  - mDNS (http://<hostname>.local) et mise à jour OTA (ArduinoOTA).
#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>

class Network {
 public:
  void begin();
  void loop();

  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  bool apActive() const { return _apActive; }
  String apName() const;

  // Enregistre de nouveaux identifiants et tente la connexion (au prochain loop).
  void reconnect() { _reconnectRequested = true; }

 private:
  void startStation();
  void startAccessPoint();
  void stopAccessPoint();
  void startServices();

  DNSServer _dns;
  bool _apActive = false;
  bool _servicesStarted = false;
  bool _reconnectRequested = false;
  bool _connecting = false;
  uint32_t _connectStart = 0;
  uint32_t _lastAttempt = 0;
  uint32_t _disconnectedSince = 0;
};

extern Network network;
