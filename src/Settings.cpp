#include "Settings.h"

#include <LittleFS.h>

#include "config.h"

Settings settings;

String Settings::deviceId() const {
  char buf[16];
  snprintf(buf, sizeof(buf), "mcz-%06x", ESP.getChipId());
  return String(buf);
}

void Settings::setDefaults() {
  wifi = WifiSettings();
  wifi.hostname = "poele-mcz";

  mqtt = MqttSettings();
  mqtt.port = DEFAULT_MQTT_PORT;
  mqtt.baseTopic = "mcz/" + deviceId();
  mqtt.discoveryPrefix = "homeassistant";

  radio.frequencyHz = DEFAULT_FREQUENCY_HZ;
  radio.highPower = DEFAULT_HIGH_POWER;
  radio.powerDbm = DEFAULT_POWER_DBM;
  radio.chipUs = DEFAULT_CHIP_US;
  radio.repeats = DEFAULT_REPEATS;

  stove = StoveSettings();
  // Identifiant propre à chaque module (dérivé de la puce) : le poêle doit être
  // appairé avec, ou on saisit celui de la télécommande d'origine s'il est connu.
  stove.id = ESP.getChipId() & 0xFFFFFF;
  stove.resendSeconds = DEFAULT_RESEND_S;

  webPassword = "";
}

// Copie un champ JSON s'il est présent et du bon type.
template <typename T>
static void readField(JsonObjectConst obj, const char* key, T& dst) {
  JsonVariantConst v = obj[key];
  if (!v.isNull() && v.is<T>()) dst = v.as<T>();
}

static void readSecret(JsonObjectConst obj, const char* key, String& dst) {
  JsonVariantConst v = obj[key];
  if (v.is<const char*>() && strlen(v.as<const char*>()) > 0) dst = v.as<String>();
}

static void writeAll(const Settings& s, JsonObject root, bool withSecrets) {
  JsonObject w = root["wifi"].to<JsonObject>();
  w["ssid"] = s.wifi.ssid;
  w["hostname"] = s.wifi.hostname;
  if (withSecrets) w["password"] = s.wifi.password;
  else w["has_password"] = s.wifi.password.length() > 0;

  JsonObject m = root["mqtt"].to<JsonObject>();
  m["enabled"] = s.mqtt.enabled;
  m["host"] = s.mqtt.host;
  m["port"] = s.mqtt.port;
  m["user"] = s.mqtt.user;
  m["base_topic"] = s.mqtt.baseTopic;
  m["discovery_prefix"] = s.mqtt.discoveryPrefix;
  if (withSecrets) m["password"] = s.mqtt.password;
  else m["has_password"] = s.mqtt.password.length() > 0;

  JsonObject r = root["radio"].to<JsonObject>();
  r["frequency_hz"] = s.radio.frequencyHz;
  r["high_power"] = s.radio.highPower;
  r["power_dbm"] = s.radio.powerDbm;
  r["chip_us"] = s.radio.chipUs;
  r["repeats"] = s.radio.repeats;

  JsonObject st = root["stove"].to<JsonObject>();
  st["id"] = s.stove.id;
  st["fan2"] = s.stove.fan2;
  st["resend_s"] = s.stove.resendSeconds;

  if (withSecrets) root["web_password"] = s.webPassword;
  else root["has_web_password"] = s.webPassword.length() > 0;
}

void Settings::toJson(JsonObject obj) const { writeAll(*this, obj, false); }

void Settings::fromJson(JsonObjectConst root) {
  JsonObjectConst w = root["wifi"];
  if (!w.isNull()) {
    readField(w, "ssid", wifi.ssid);
    readField(w, "hostname", wifi.hostname);
    readSecret(w, "password", wifi.password);
  }
  JsonObjectConst m = root["mqtt"];
  if (!m.isNull()) {
    readField(m, "enabled", mqtt.enabled);
    readField(m, "host", mqtt.host);
    readField(m, "port", mqtt.port);
    readField(m, "user", mqtt.user);
    readField(m, "base_topic", mqtt.baseTopic);
    readField(m, "discovery_prefix", mqtt.discoveryPrefix);
    readSecret(m, "password", mqtt.password);
    if (m["clear_password"] | false) mqtt.password = "";
  }
  JsonObjectConst r = root["radio"];
  if (!r.isNull()) {
    readField(r, "frequency_hz", radio.frequencyHz);
    readField(r, "high_power", radio.highPower);
    readField(r, "power_dbm", radio.powerDbm);
    readField(r, "chip_us", radio.chipUs);
    readField(r, "repeats", radio.repeats);
  }
  JsonObjectConst st = root["stove"];
  if (!st.isNull()) {
    readField(st, "id", stove.id);
    readField(st, "fan2", stove.fan2);
    readField(st, "resend_s", stove.resendSeconds);
  }
  readSecret(root, "web_password", webPassword);
  if (root["clear_web_password"] | false) webPassword = "";

  // garde-fous
  if (wifi.hostname.isEmpty()) wifi.hostname = "poele-mcz";
  while (mqtt.baseTopic.endsWith("/")) mqtt.baseTopic.remove(mqtt.baseTopic.length() - 1);
  if (mqtt.baseTopic.isEmpty()) mqtt.baseTopic = "mcz/" + deviceId();
  if (mqtt.discoveryPrefix.isEmpty()) mqtt.discoveryPrefix = "homeassistant";
  radio.frequencyHz = constrain(radio.frequencyHz, 863000000UL, 870000000UL);
  radio.powerDbm = constrain(radio.powerDbm, (int8_t)-18, (int8_t)17);
  radio.chipUs = constrain(radio.chipUs, (uint16_t)100, (uint16_t)2000);
  radio.repeats = constrain(radio.repeats, (uint8_t)1, (uint8_t)5);
  stove.id &= 0xFFFFFF;
  stove.fan2 &= 0x07;
}

bool Settings::load() {
  setDefaults();
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  fromJson(doc.as<JsonObjectConst>());
  // les secrets vides doivent pouvoir être relus tels quels
  wifi.password = doc["wifi"]["password"] | "";
  mqtt.password = doc["mqtt"]["password"] | "";
  webPassword = doc["web_password"] | "";
  return true;
}

bool Settings::save() const {
  JsonDocument doc;
  writeAll(*this, doc.to<JsonObject>(), true);
  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

void Settings::clearWifi() {
  wifi.ssid = "";
  wifi.password = "";
  save();
}
