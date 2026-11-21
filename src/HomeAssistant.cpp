#include "HomeAssistant.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "Settings.h"
#include "Stove.h"
#include "config.h"

static const uint32_t RECONNECT_INTERVAL_MS = 10000;
static const uint32_t DIAG_INTERVAL_MS = 60000;

HomeAssistant::HomeAssistant(Stove& stove) : _stove(stove), _mqtt(_net) {}

String HomeAssistant::topic(const char* suffix) const { return settings.mqtt.baseTopic + suffix; }

void HomeAssistant::begin() {
  _mqtt.setBufferSize(1024);  // messages de découverte HA
  _mqtt.setCallback([this](char* t, uint8_t* p, unsigned int l) { onMessage(t, p, l); });
  _stove.onChange([this]() { _statePending = true; });
  reconfigure();
}

void HomeAssistant::reconfigure() {
  if (_mqtt.connected()) {
    _mqtt.publish(topic("/status").c_str(), "offline", true);
    _mqtt.disconnect();
  }
  _mqtt.setServer(settings.mqtt.host.c_str(), settings.mqtt.port);
  _lastAttempt = 0;
  _lastError = "";
}

bool HomeAssistant::connect() {
  String clientId = settings.deviceId();
  String willTopic = topic("/status");
  const char* user = settings.mqtt.user.length() ? settings.mqtt.user.c_str() : nullptr;
  const char* pass = settings.mqtt.password.length() ? settings.mqtt.password.c_str() : nullptr;

  Serial.printf("[MQTT] Connexion à %s:%u...\n", settings.mqtt.host.c_str(), settings.mqtt.port);
  if (!_mqtt.connect(clientId.c_str(), user, pass, willTopic.c_str(), 1, true, "offline")) {
    _lastError = "échec de connexion (code " + String(_mqtt.state()) + ")";
    Serial.printf("[MQTT] %s\n", _lastError.c_str());
    return false;
  }
  _lastError = "";
  Serial.println("[MQTT] Connecté");
  _mqtt.publish(willTopic.c_str(), "online", true);
  _mqtt.subscribe(topic("/set").c_str());
  _mqtt.subscribe(topic("/set/+").c_str());
  _mqtt.subscribe((settings.mqtt.discoveryPrefix + "/status").c_str());
  publishDiscovery();
  publishState();
  publishDiagnostics();
  return true;
}

void HomeAssistant::loop() {
  if (!settings.mqtt.enabled || settings.mqtt.host.isEmpty() || WiFi.status() != WL_CONNECTED) {
    if (_mqtt.connected()) _mqtt.disconnect();
    return;
  }
  if (!_mqtt.connected()) {
    if (_lastAttempt == 0 || millis() - _lastAttempt > RECONNECT_INTERVAL_MS) {
      _lastAttempt = millis();
      connect();
    }
    return;
  }
  _mqtt.loop();
  if (_statePending) publishState();
  if (millis() - _lastDiag > DIAG_INTERVAL_MS) publishDiagnostics();
}

void HomeAssistant::onMessage(char* rawTopic, uint8_t* payload, unsigned int len) {
  String t(rawTopic);
  String value;
  value.reserve(len);
  for (unsigned int i = 0; i < len; i++) value += (char)payload[i];
  value.trim();
  Serial.printf("[MQTT] %s <- %s\n", rawTopic, value.c_str());

  if (t == settings.mqtt.discoveryPrefix + "/status") {
    if (value == "online") {  // Home Assistant vient de redémarrer
      publishDiscovery();
      publishState();
    }
    return;
  }

  const String base = topic("/set");
  if (t == base) {
    JsonDocument doc;
    String err;
    if (deserializeJson(doc, value) || !_stove.applyJson(doc.as<JsonObjectConst>(), err))
      Serial.printf("[MQTT] Commande refusée : %s\n", err.c_str());
    return;
  }
  if (!t.startsWith(base + "/")) return;
  String cmd = t.substring(base.length() + 1);
  value.toLowerCase();

  bool ok = true;
  if (cmd == "switch") {
    if (value == "on") _stove.setOn(true);
    else if (value == "off") _stove.setOn(false);
    else ok = false;
  } else if (cmd == "power") {
    ok = _stove.setPower(value.toInt());
  } else if (cmd == "fan") {
    ok = _stove.setFan(value == "auto" ? mcz::FAN_AUTO : value.toInt());
  } else if (cmd == "resend") {
    _stove.resend();
  } else {
    ok = false;
  }
  if (!ok) {
    Serial.println("[MQTT] Commande refusée");
    _statePending = true;  // rétablit l'état affiché dans HA
  }
}

void HomeAssistant::publishState() {
  _statePending = false;
  JsonDocument doc;
  _stove.toJson(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  _mqtt.publish(topic("/state").c_str(), out.c_str(), true);
}

void HomeAssistant::publishDiagnostics() {
  _lastDiag = millis();
  JsonDocument doc;
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime_s"] = millis() / 1000;
  doc["version"] = FIRMWARE_VERSION;
  String out;
  serializeJson(doc, out);
  _mqtt.publish(topic("/diag").c_str(), out.c_str(), true);
}

namespace {
// Champs communs à toutes les entités.
void fillCommon(JsonDocument& doc, const char* objectId, const char* name) {
  const String id = settings.deviceId();
  const String base = settings.mqtt.baseTopic;
  doc["name"] = name;
  doc["unique_id"] = id + "_" + objectId;
  doc["availability_topic"] = base + "/status";

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(id);
  dev["name"] = "Poêle MCZ";
  dev["manufacturer"] = "MCZ";
  dev["model"] = "Ego Air 2.0 - télécommande WiFi";
  dev["sw_version"] = FIRMWARE_VERSION;
  dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
}
}  // namespace

void HomeAssistant::publishDiscovery() {
  const String base = settings.mqtt.baseTopic;
  const String prefix = settings.mqtt.discoveryPrefix + "/";
  const String node = "/" + settings.deviceId() + "/";
  String out;

  auto send = [&](const char* component, const char* objectId, JsonDocument& doc) {
    out = "";
    serializeJson(doc, out);
    String t = prefix + component + node + objectId + "/config";
    if (!_mqtt.publish(t.c_str(), out.c_str(), true))
      Serial.printf("[MQTT] Découverte trop longue : %s\n", t.c_str());
  };

  {
    JsonDocument doc;
    fillCommon(doc, "switch", "Poêle");
    doc["icon"] = "mdi:fireplace";
    doc["state_topic"] = base + "/state";
    doc["value_template"] = "{{ 'ON' if value_json.on else 'OFF' }}";
    doc["command_topic"] = base + "/set/switch";
    send("switch", "switch", doc);
  }
  // Le sélecteur "Mode" des versions précédentes est supprimé (sans effet sur
  // le poêle) : un message retenu vide efface l'entité dans Home Assistant.
  _mqtt.publish((prefix + "select" + node + "mode/config").c_str(), "", true);

  {
    JsonDocument doc;
    fillCommon(doc, "power", "Puissance");
    doc["icon"] = "mdi:fire";
    doc["state_topic"] = base + "/state";
    doc["value_template"] = "{{ value_json.power }}";
    doc["command_topic"] = base + "/set/power";
    doc["min"] = mcz::POWER_MIN;
    doc["max"] = mcz::POWER_MAX;
    doc["step"] = 1;
    doc["mode"] = "slider";
    send("number", "power", doc);
  }
  {
    JsonDocument doc;
    fillCommon(doc, "fan", "Ventilation");
    doc["icon"] = "mdi:fan";
    doc["state_topic"] = base + "/state";
    doc["value_template"] = "{{ value_json.fan | string }}";
    doc["command_topic"] = base + "/set/fan";
    JsonArray o = doc["options"].to<JsonArray>();
    for (const char* v : {"1", "2", "3", "4", "5", "auto"}) o.add(v);
    send("select", "fan", doc);
  }
  {
    JsonDocument doc;
    fillCommon(doc, "resend", "Renvoyer la commande");
    doc["icon"] = "mdi:broadcast";
    doc["command_topic"] = base + "/set/resend";
    doc["payload_press"] = "1";
    send("button", "resend", doc);
  }
  {
    JsonDocument doc;
    fillCommon(doc, "rssi", "Signal WiFi");
    doc["state_topic"] = base + "/diag";
    doc["value_template"] = "{{ value_json.rssi }}";
    doc["device_class"] = "signal_strength";
    doc["unit_of_measurement"] = "dBm";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    send("sensor", "rssi", doc);
  }
}
