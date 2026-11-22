#include "WebUi.h"

#include <LittleFS.h>
#include <Updater.h>
#include <flash_hal.h>
#include <Rfm69Tx.h>

#include "Autonomous.h"
#include "Clock.h"
#include "HomeAssistant.h"
#include "Network.h"
#include "Settings.h"
#include "Stove.h"
#include "TempSensor.h"
#include "config.h"

static const char* const WEB_USER = "admin";

// Page affichée si le système de fichiers n'a pas été téléversé.
static const char FALLBACK_PAGE[] PROGMEM =
    "<!doctype html><meta charset=utf-8><title>MCZ WiFi</title>"
    "<h1>Interface web absente</h1><p>Le système de fichiers n'a pas été téléversé. "
    "Lancez <code>pio run -t uploadfs</code>, ou utilisez l'API : <code>/api/status</code>.</p>"
    "<p><a href=/update>Mise à jour</a></p>";

// Fichiers personnels conservés lors de l'envoi d'une nouvelle interface web.
static const char* const PRIVATE_FILES[] = {SETTINGS_FILE, STATE_FILE, SCHEDULE_FILE};

static const char UPDATE_PAGE[] PROGMEM = R"(<!doctype html><html lang=fr><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Mise à jour MCZ WiFi</title>
<style>body{font-family:system-ui,sans-serif;max-width:560px;margin:0 auto;padding:16px;line-height:1.45}
form{border:1px solid #ccc;border-radius:10px;padding:14px;margin:14px 0}h2{font-size:1.05em;margin:0 0 6px}
p{margin:6px 0;color:#555}input,button{font:inherit;margin-top:8px}button{padding:6px 14px}
#msg{font-weight:600}</style>
<h1>Mise à jour</h1><p><a href=/>‹ Retour</a></p>
<form method=POST enctype=multipart/form-data><h2>Firmware</h2>
<p>Fichier <code>mcz-wifi-x.y.z-firmware.bin</code>.</p>
<input type=file accept=.bin,.bin.gz name=firmware required><br><button>Envoyer le firmware</button></form>
<form method=POST enctype=multipart/form-data><h2>Interface web</h2>
<p>Fichier <code>mcz-wifi-x.y.z-interface.bin</code>, après le firmware de la même version.
Les réglages, l'état du poêle et la programmation sont conservés.</p>
<input type=file accept=.bin name=filesystem required><br><button>Envoyer l'interface</button></form>
<p id=msg></p>
<script>document.querySelectorAll("form").forEach(f=>f.addEventListener("submit",()=>{
document.querySelectorAll("button").forEach(b=>b.disabled=true);
document.getElementById("msg").textContent="Envoi en cours, ne fermez pas la page…"}))</script>)";

void WebUi::begin() {
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/stove", HTTP_POST, [this]() { handleStove(); });
  _server.on("/api/stove/resend", HTTP_POST, [this]() { handleResend(); });
  _server.on("/api/auto", HTTP_POST, [this]() { handleAuto(); });
  _server.on("/api/schedule", HTTP_GET, [this]() { handleGetSchedule(); });
  _server.on("/api/schedule", HTTP_POST, [this]() { handlePostSchedule(); });
  _server.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
  _server.on("/api/settings", HTTP_POST, [this]() { handlePostSettings(); });
  _server.on("/api/wifi/scan", HTTP_GET, [this]() { handleScan(); });
  _server.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  _server.on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
  _server.onNotFound([this]() { handleNotFound(); });

  // Mise à jour par le navigateur (protégée par le mot de passe web s'il existe).
  _server.on("/update", HTTP_GET, [this]() { handleUpdatePage(); });
  _server.on("/update", HTTP_POST, [this]() { handleUpdateDone(); }, [this]() { handleUpdateUpload(); });

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
  autonomous.statusToJson(doc["auto"].to<JsonObject>());

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

  JsonObject t = dev["temperature"].to<JsonObject>();
  t["present"] = _temp.present();
  if (_temp.valid()) {
    t["celsius"] = _temp.celsius();
    t["age_s"] = _temp.ageMs() / 1000;
  }
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

void WebUi::handleAuto() {
  if (!authenticate()) return;
  JsonDocument body;
  if (!readJsonBody(body)) return;
  if (!body["enabled"].is<bool>()) return sendError(400, "\"enabled\" doit valoir true ou false");
  autonomous.setEnabled(body["enabled"].as<bool>());
  JsonDocument doc;
  autonomous.statusToJson(doc.to<JsonObject>());
  sendJson(200, doc);
}

void WebUi::handleGetSchedule() {
  if (!authenticate()) return;
  JsonDocument doc;
  autonomous.scheduleToJson(doc.to<JsonObject>());
  sendJson(200, doc);
}

void WebUi::handlePostSchedule() {
  if (!authenticate()) return;
  JsonDocument body;
  if (!readJsonBody(body)) return;
  String err;
  if (!autonomous.scheduleFromJson(body.as<JsonObjectConst>(), err)) return sendError(400, err);
  JsonDocument doc;
  autonomous.scheduleToJson(doc.to<JsonObject>());
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
  if (!body["time"].isNull()) sysClock.begin();

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
  LittleFS.remove(SCHEDULE_FILE);
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
  if (path == "/index.html" || path == "/settings.html" || path == "/schedule.html") {
    _server.send_P(200, "text/html", FALLBACK_PAGE);
    return;
  }
  _server.send(404, "text/plain", "Introuvable");
}

// --- mise à jour -----------------------------------------------------------

void WebUi::handleUpdatePage() {
  if (!authenticate()) return;
  _server.send_P(200, "text/html; charset=utf-8", UPDATE_PAGE);
}

void WebUi::handleUpdateDone() {
  if (!_updAuth) return _server.requestAuthentication(BASIC_AUTH, "MCZ WiFi");
  _updAuth = false;
  if (_updError.length() || Update.hasError()) {
    String msg = _updError.length() ? _updError : String("échec");
    _server.send(500, "text/html; charset=utf-8",
                 "<meta charset=utf-8><p>Échec de la mise à jour : " + msg + "</p><p><a href=/update>Retour</a></p>");
    if (_updFs) _rebootAt = millis() + 1000;  // interface incomplète : repartir proprement
    return;
  }
  _server.send(200, "text/html; charset=utf-8",
               "<meta charset=utf-8><meta http-equiv=refresh content=\"15;URL=/\">"
               "<p>Mise à jour réussie, redémarrage du module…</p>");
  _rebootAt = millis() + 500;
}

void WebUi::keepPrivateFiles() {
  _kept.clear();
  for (const char* path : PRIVATE_FILES) {
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    _kept.emplace_back(path, f.readString());
    f.close();
  }
}

size_t WebUi::restorePrivateFiles() {
  size_t n = 0;
  if (LittleFS.begin()) {
    for (auto& kv : _kept) {
      File f = LittleFS.open(kv.first, "w");
      if (!f) continue;
      if (f.print(kv.second) == kv.second.length()) n++;
      f.close();
    }
  }
  _kept.clear();
  return n;
}

void WebUi::handleUpdateUpload() {
  HTTPUpload& up = _server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _updError = "";
    _updFs = false;
    _updAuth = authenticate();
    if (!_updAuth) return;
    _updFs = up.name == "filesystem";
    Serial.printf("[MAJ] %s : %s\n", _updFs ? "interface" : "firmware", up.filename.c_str());
    bool ok;
    if (_updFs) {
      keepPrivateFiles();
      Serial.printf("[MAJ] %u fichier(s) de réglages conservé(s) en mémoire\n", (unsigned)_kept.size());
      LittleFS.end();
      ok = Update.begin((size_t)FS_end - (size_t)FS_start, U_FS);
    } else {
      ok = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FLASH);
    }
    if (!ok) _updError = Update.getErrorString();
  } else if (!_updAuth || _updError.length()) {
    if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
      if (_updFs) restorePrivateFiles();
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) _updError = Update.getErrorString();
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) _updError = Update.getErrorString();
    Serial.printf("[MAJ] %s (%u octets)\n", _updError.length() ? _updError.c_str() : "terminée",
                  (unsigned)up.totalSize);
    if (_updFs) {
      // Le système de fichiers vient d'être remplacé (ou est incomplet en cas
      // d'échec) : les réglages sont réécrits dans tous les cas.
      size_t n = restorePrivateFiles();
      Serial.printf("[MAJ] %u fichier(s) de réglages restauré(s)\n", (unsigned)n);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    _updError = "envoi interrompu";
    if (_updFs) restorePrivateFiles();
  }
  delay(0);
}
