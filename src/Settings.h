// Paramètres persistants (LittleFS, /config.json).
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct WifiSettings {
  String ssid;
  String password;
  String hostname;
};

struct MqttSettings {
  bool enabled = false;
  String host;
  uint16_t port;
  String user;
  String password;
  String baseTopic;        // ex. "mcz/poele"
  String discoveryPrefix;  // "homeassistant"
};

struct RadioSettings {
  uint32_t frequencyHz;
  bool highPower;  // false = RFM69W/CW, true = RFM69HW/HCW
  int8_t powerDbm;
  uint16_t chipUs;
  uint8_t repeats;
};

struct TimeSettings {
  String tz;   // fuseau horaire POSIX
  String ntp;  // serveur NTP
};

// Paramètres de régulation du mode autonome (voir lib/Thermostat).
struct AutoSettings {
  float hysteresis = 1.0;    // allumage à consigne - écart (°C)
  uint8_t startupPower = 5;
  uint16_t startupMin = 20;
  uint8_t heatPower = 3;
  uint8_t holdPower = 1;
  uint16_t holdMin = 15;
  bool beep = true;          // bip du poêle aux allumages / arrêts automatiques
};

struct SensorSettings {
  float offset = 0;  // correction de la sonde de température (°C)
};

struct StoveSettings {
  uint32_t id;
  uint8_t fan2 = 7;        // 7 = absent (poêle à 1 ventilateur)
  uint16_t resendSeconds;  // renvoi périodique silencieux (0 = désactivé)
};

class Settings {
 public:
  WifiSettings wifi;
  MqttSettings mqtt;
  RadioSettings radio;
  StoveSettings stove;
  SensorSettings sensor;
  TimeSettings time;
  AutoSettings autoMode;
  String webPassword;  // vide = pas d'authentification sur l'interface web

  void setDefaults();
  bool load();
  bool save() const;

  // Sérialisation pour l'API web. Les mots de passe ne sont jamais renvoyés.
  void toJson(JsonObject obj) const;
  // Applique un JSON partiel (les champs absents sont conservés, un mot de
  // passe vide est ignoré pour ne pas effacer la valeur enregistrée).
  void fromJson(JsonObjectConst obj);

  void clearWifi();
  String deviceId() const;  // identifiant unique basé sur l'ID puce
};

extern Settings settings;
