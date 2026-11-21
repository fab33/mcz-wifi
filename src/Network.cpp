#include "Network.h"

#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

#include "Settings.h"
#include "config.h"

Network network;

String Network::apName() const {
  char buf[16];
  snprintf(buf, sizeof(buf), "MCZ-%06X", ESP.getChipId());
  return String(buf);
}

void Network::begin() {
  WiFi.persistent(false);  // les identifiants sont gérés dans /config.json
  WiFi.setAutoReconnect(true);
  WiFi.hostname(settings.wifi.hostname);
  if (settings.wifi.ssid.isEmpty()) {
    startAccessPoint();
  } else {
    startStation();
  }
}

void Network::startStation() {
  Serial.printf("[WiFi] Connexion à \"%s\"...\n", settings.wifi.ssid.c_str());
  WiFi.mode(_apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(settings.wifi.ssid, settings.wifi.password);
  _connecting = true;
  _connectStart = millis();
  _lastAttempt = millis();
}

void Network::startAccessPoint() {
  if (_apActive) return;
  WiFi.mode(WIFI_AP_STA);  // la partie station est nécessaire pour scanner les réseaux
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName(), AP_PASSWORD);
  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  _dns.start(53, "*", WiFi.softAPIP());
  _apActive = true;
  Serial.printf("[WiFi] Point d'accès \"%s\" (mot de passe \"%s\") -> http://192.168.4.1\n",
                apName().c_str(), AP_PASSWORD);
}

void Network::stopAccessPoint() {
  if (!_apActive) return;
  _dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  _apActive = false;
  Serial.println("[WiFi] Point d'accès arrêté");
}

void Network::startServices() {
  if (_servicesStarted) return;
  MDNS.begin(settings.wifi.hostname);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(settings.wifi.hostname.c_str());
  if (settings.webPassword.length()) ArduinoOTA.setPassword(settings.webPassword.c_str());
  ArduinoOTA.begin();
  _servicesStarted = true;
}

void Network::loop() {
  if (_apActive) _dns.processNextRequest();
  if (_servicesStarted) {
    MDNS.update();
    ArduinoOTA.handle();
  }

  if (_reconnectRequested) {
    _reconnectRequested = false;
    WiFi.disconnect();
    WiFi.hostname(settings.wifi.hostname);
    if (settings.wifi.ssid.isEmpty()) startAccessPoint();
    else startStation();
    return;
  }

  if (connected()) {
    _disconnectedSince = 0;
    if (_connecting) {
      _connecting = false;
      Serial.printf("[WiFi] Connecté, IP %s\n", WiFi.localIP().toString().c_str());
      startServices();
      // On laisse le temps à la page de configuration d'afficher la nouvelle IP.
    }
    if (_apActive && millis() - _connectStart > 30000) stopAccessPoint();
    return;
  }

  if (_connecting && millis() - _connectStart > WIFI_CONNECT_TIMEOUT_MS) {
    _connecting = false;
    Serial.println("[WiFi] Échec de connexion");
    startAccessPoint();
  }

  // Portail ouvert : on retente régulièrement le réseau mémorisé (ex. box
  // redémarrée après une coupure de courant).
  if (!_connecting && _apActive && !settings.wifi.ssid.isEmpty() &&
      millis() - _lastAttempt > WIFI_RETRY_INTERVAL_MS && WiFi.softAPgetStationNum() == 0) {
    startStation();
  }

  // Perte de connexion en fonctionnement normal : l'ESP se reconnecte seul ;
  // si cela dure, on ouvre le portail.
  if (!_connecting && !_apActive) {
    if (_disconnectedSince == 0) {
      _disconnectedSince = millis();
    } else if (millis() - _disconnectedSince > WIFI_CONNECT_TIMEOUT_MS * 3) {
      _lastAttempt = millis();
      startAccessPoint();
    }
  }
}
