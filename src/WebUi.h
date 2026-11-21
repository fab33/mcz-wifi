// Serveur web : pages statiques (data/ -> LittleFS) et API REST JSON.
//
//   GET  /api/status          état du poêle + infos module
//   POST /api/stove           {"on":true,"power":3,"fan":"auto"}
//   POST /api/stove/resend    renvoie la dernière commande
//   GET  /api/settings        réglages (sans les mots de passe)
//   POST /api/settings        réglages partiels
//   GET  /api/wifi/scan       réseaux WiFi visibles
//   POST /api/reboot
//   POST /api/factory-reset   efface tous les réglages
//   GET  /update              mise à jour du firmware par le navigateur
#pragma once

#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>

class Stove;
class HomeAssistant;
class Rfm69Tx;

class WebUi {
 public:
  WebUi(Stove& stove, HomeAssistant& ha, Rfm69Tx& radio)
      : _stove(stove), _ha(ha), _radio(radio), _server(80) {}

  void begin();
  void loop();

 private:
  bool authenticate();
  bool readJsonBody(JsonDocument& doc);
  void sendJson(int code, const JsonDocument& doc);
  void sendError(int code, const String& msg);
  bool captivePortalRedirect();

  void handleStatus();
  void handleStove();
  void handleResend();
  void handleGetSettings();
  void handlePostSettings();
  void handleScan();
  void handleReboot();
  void handleFactoryReset();
  void handleNotFound();

  Stove& _stove;
  HomeAssistant& _ha;
  Rfm69Tx& _radio;
  ESP8266WebServer _server;
  ESP8266HTTPUpdateServer _updater;
  uint32_t _rebootAt = 0;
};
