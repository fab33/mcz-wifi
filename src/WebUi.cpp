#include "WebUi.h"

#include <LittleFS.h>
#include <Rfm69Tx.h>

#include "HomeAssistant.h"
#include "Network.h"
#include "Settings.h"
#include "Stove.h"
#include "config.h"

static const char* const WEB_USER = "admin";

// Page affichée si le système de fichiers n'a pas été téléversé.
static const char FALLBACK_PAGE[] PROGMEM =
    "<!doctype html><meta charset=utf-8><title>MCZ WiFi</title>"
    "<h1>Interface web absente</h1><p>Le système de fichiers n'a pas été téléversé. "
    "Lancez <code>pio run -t uploadfs</code>, ou utilisez l'API : <code>/api/status</code>.</p>"
    "<p><a href=/update>Mise à jour du firmware</a></p>";

void WebUi::begin() {
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/stove", HTTP_POST, [this]() { handleStove(); });
  _server.on("/api/stove/resend", HTTP_POST, [this]() { handleResend(); });
  _server.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
  _server.on("/api/settings", HTTP_POST, [this]() { handlePostSettings(); });
  _server.on("/api/wifi/scan", HTTP_GET, [this]() { handleScan(); });
  _server.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  _server.on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
  _server.onNotFound([this]() { handleNotFound(); });

  // Mise à jour OTA par le navigateur (protégée par le mot de passe web s'il existe).
  if (settings.webPassword.length()) _updater.setup(&_server, "/update", WEB_USER, settings.webPassword);
  else _updater.setup(&_server, "/update");

  _server.begin();
}

void WebUi::loop() {
  _server.handleClient();
  if (_rebootAt && millis() > _rebootAt) ESP.restart();
}

// --- utilitaires -----------------------------------------------------------

bool WebUi::authenticate() {
  if (settings.webPassword.isEmpty()) return true;
  if (_server.authenticate(WEB_USER, settings.webPassword.c_str())) return true;
  _server.requestAuthentication(BASIC_AUTH, "MCZ WiFi");
  return false;
}

bool WebUi::readJsonBody(JsonDocument& doc) {
  if (!_server.hasArg("plain") || deserializeJson(doc, _server.arg("plain")) || !doc.is<JsonObject>()) {
    sendError(400, "JSON invalide");
    return false;
  }
  return true;
}

void WebUi::sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(code, "application/json", out);
}

void WebUi::sendError(int code, const String& msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(code, doc);
}

bool WebUi::captivePortalRedirect() {
  if (!network.apActive()) return false;
  String host = _server.hostHeader();
  if (host == WiFi.softAPIP().toString() || host.endsWith(".local")) return false;
  _server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings.html", true);
  _server.send(302, "text/plain", "");
  return true;
}

// --- routes ------------------------------------------------------------------

void WebUi::handleStatus() {
  if (!authenticate()) return;
  JsonDocument doc;
  _stove.toJson(doc["stove"].to<JsonObject>());

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["version"] = FIRMWARE_VERSION;
  dev["uptime_s"] = millis() / 1000;
  dev["free_heap"] = ESP.getFreeHeap();
  dev["hostname"] = settings.wifi.hostname;

  JsonObject w = dev["wifi"].to<JsonObject>();
  w["connected"] = network.connected();
  w["ssid"] = WiFi.SSID();
  w["ip"] = WiFi.localIP().toString();
  w["rssi"] = WiFi.RSSI();
  w["ap"] = network.apActive();
  w["ap_name"] = network.apName();

  JsonObject m = dev["mqtt"].to<JsonObject>();
  m["enabled"] = settings.mqtt.enabled;
  m["connected"] = _ha.connected();
  m["error"] = _ha.lastError();

  JsonObject r = dev["radio"].to<JsonObject>();
  r["present"] = _radio.present();
  if (_radio.present()) r["version"] = _radio.version();
  sendJson(200, doc);
}

void WebUi::handleStove() {
  if (!authenticate()) return;
  JsonDocument body;
  if (!readJsonBody(body)) return;
  String err;
  if (!_stove.applyJson(body.as<JsonObjectConst>(), err)) return sendError(400, err);
  JsonDocument doc;
  _stove.toJson(doc.to<JsonObject>());
  sendJson(200, doc);
}

void WebUi::handleResend() {
  if (!authenticate()) return;
  _stove.resend();
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
}

void WebUi::handleGetSettings() {
  if (!authenticate()) return;
  JsonDocument doc;
  settings.toJson(doc.to<JsonObject>());
  sendJson(200, doc);
}

void WebUi::handlePostSettings() {
  if (!authenticate()) return;
  JsonDocument body;
  if (!readJsonBody(body)) return;

  const String oldSsid = settings.wifi.ssid, oldPass = settings.wifi.password;
  const String oldHost = settings.wifi.hostname, oldWebPass = settings.webPassword;
  settings.fromJson(body.as<JsonObjectConst>());
  if (!settings.save()) return sendError(500, "écriture impossible");

  _stove.applyRadioSettings();
  if (!body["mqtt"].isNull()) _ha.reconfigure();

  bool wifiChanged = settings.wifi.ssid != oldSsid || settings.wifi.password != oldPass;
  bool needsReboot = settings.wifi.hostname != oldHost || settings.webPassword != oldWebPass;

  JsonDocument doc;
  doc["ok"] = true;
  doc["wifi_changed"] = wifiChanged;
  doc["reboot"] = needsReboot;
  sendJson(200, doc);

  if (needsReboot) _rebootAt = millis() + 1500;
  else if (wifiChanged) network.reconnect();
}

void WebUi::handleScan() {
  if (!authenticate()) return;
  JsonDocument doc;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);  // scan asynchrone, le navigateur rappellera
    doc["scanning"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    doc["scanning"] = true;
  } else {
    doc["scanning"] = false;
    JsonArray list = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      bool dup = false;
      for (JsonObject o : list) dup |= o["ssid"] == WiFi.SSID(i);
      if (dup || WiFi.SSID(i).isEmpty()) continue;
      JsonObject o = list.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secure"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
    }
    WiFi.scanDelete();
  }
  sendJson(200, doc);
}

void WebUi::handleReboot() {
  if (!authenticate()) return;
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
  _rebootAt = millis() + 1000;
}

void WebUi::handleFactoryReset() {
  if (!authenticate()) return;
  LittleFS.remove(SETTINGS_FILE);
  LittleFS.remove(STATE_FILE);
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
  _rebootAt = millis() + 1000;
}

static const char* contentType(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

void WebUi::handleNotFound() {
  if (captivePortalRedirect()) return;
  if (!authenticate()) return;

  String path = _server.uri();
  if (path.endsWith("/")) path += network.apActive() && !network.connected() ? "settings.html" : "index.html";

  // fichiers compressés (.gz) servis en priorité s'ils existent
  String gz = path + ".gz";
  bool useGz = LittleFS.exists(gz);
  if (useGz || LittleFS.exists(path)) {
    File f = LittleFS.open(useGz ? gz : path, "r");
    _server.sendHeader("Cache-Control", "max-age=300");
    _server.streamFile(f, contentType(path));
    f.close();
    return;
  }
  if (path == "/index.html" || path == "/settings.html") {
    _server.send_P(200, "text/html", FALLBACK_PAGE);
    return;
  }
  _server.send(404, "text/plain", "Introuvable");
}
