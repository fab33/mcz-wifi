// Télécommande WiFi pour poêle à granulés MCZ (Ego Air 2.0)
// Matériel : Wemos D1 mini + shield WeMos RFM69 (ch2i) avec RFM69 868 MHz.
//
// Organisation :
//   lib/MczProtocol  encodage des trames MCZ (C++ pur, testé sur PC)
//   lib/Rfm69Tx      pilote d'émission OOK du RFM69
//   src/Settings     réglages persistants (LittleFS)
//   src/Stove        état du poêle et émission des commandes
//   src/Network      WiFi, point d'accès de configuration, mDNS, OTA
//   src/WebUi        serveur web et API REST
//   src/HomeAssistant MQTT + découverte Home Assistant
//   src/Peripherals  LEDs et bouton du shield
//   data/            pages web (HTML / CSS / JS)
#include <Arduino.h>
#include <LittleFS.h>
#include <Rfm69Tx.h>

#include "HomeAssistant.h"
#include "Network.h"
#include "Peripherals.h"
#include "Settings.h"
#include "Stove.h"
#include "WebUi.h"
#include "config.h"

static Rfm69Tx radio(PIN_RFM_CS);
static Stove stove(radio);
static HomeAssistant homeAssistant(stove);
static WebUi webUi(stove, homeAssistant, radio);
static Peripherals peripherals(stove, homeAssistant, radio);

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\n%s %s\n", FIRMWARE_NAME, FIRMWARE_VERSION);

  peripherals.begin();

  if (!LittleFS.begin()) {
    Serial.println("[FS] Montage impossible, formatage...");
    LittleFS.format();
    LittleFS.begin();
  }
  if (!settings.load()) Serial.println("[Config] Aucun réglage, valeurs par défaut");

  if (radio.begin()) {
    Serial.printf("[RF] RFM69 détecté (version 0x%02X)\n", radio.version());
  } else {
    Serial.println("[RF] RFM69 introuvable : vérifiez le module et le soudage");
  }
  stove.begin();

  network.begin();
  homeAssistant.begin();
  webUi.begin();
}

void loop() {
  network.loop();
  webUi.loop();
  homeAssistant.loop();
  stove.loop();
  peripherals.loop();
}
