// Serveur web : pages statiques (data/ -> LittleFS) et API REST JSON.
//
//   GET  /api/status          état du poêle + infos module (+ température si sonde)
//   POST /api/stove           {"on":true,"power":3,"fan":"auto"} (désactive le mode autonome)
//   POST /api/stove/resend    renvoie la dernière commande
//   POST /api/auto            {"enabled":true} active / désactive le mode autonome
//   GET  /api/schedule        programmation du mode autonome
//   POST /api/schedule        remplace la programmation
//   GET  /api/settings        réglages (sans les mots de passe)
//   POST /api/settings        réglages partiels
//   GET  /api/wifi/scan       réseaux WiFi visibles
//   POST /api/reboot
//   POST /api/factory-reset   efface tous les réglages
//   GET  /update              page de mise à jour (firmware ou interface web)
//   POST /update              envoi d'un fichier : champ "firmware" ou "filesystem".
//                             L'interface web (image LittleFS) remplace tout le
//                             système de fichiers : les réglages, l'état du poêle
//                             et la programmation sont gardés en mémoire pendant
//                             l'envoi puis réécrits dans la nouvelle image.
#pragma once

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include <utility>
#include <vector>

class Stove;
class HomeAssistant;
class Rfm69Tx;
class TempSensor;

class WebUi {
 public:
  WebUi(Stove& stove, HomeAssistant& ha, Rfm69Tx& radio, TempSensor& temp)
      : _stove(stove), _ha(ha), _radio(radio), _temp(temp), _server(80) {}

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
  void handleAuto();
  void handleGetSchedule();
  void handlePostSchedule();
  void handleGetSettings();
  void handlePostSettings();
  void handleScan();
  void handleReboot();
  void handleFactoryReset();
  void handleNotFound();
  void handleUpdatePage();
  void handleUpdateDone();
  void handleUpdateUpload();
  void keepPrivateFiles();
  size_t restorePrivateFiles();

  Stove& _stove;
  HomeAssistant& _ha;
  Rfm69Tx& _radio;
  TempSensor& _temp;
  ESP8266WebServer _server;
  // Mise à jour en cours
  bool _updAuth = false;
  bool _updFs = false;
  String _updError;
  std::vector<std::pair<String, String>> _kept;  // fichiers privés (chemin, contenu)
  uint32_t _rebootAt = 0;
};
