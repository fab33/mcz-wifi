// Constantes matérielles et valeurs par défaut.
#pragma once

#include <stdint.h>

#define FIRMWARE_NAME "mcz-wifi"
#define FIRMWARE_VERSION "1.2.1"

// --- Brochage du shield WeMos RFM69 (ch2i) sur Wemos D1 mini --------------
// SPI matériel : SCK = GPIO14 (D5), MISO = GPIO12 (D6), MOSI = GPIO13 (D7)
static const uint8_t PIN_RFM_CS = 16;      // D0 -> NSS du RFM69
static const uint8_t PIN_RFM_DIO0 = 15;    // D8 -> DIO0 (non utilisé)
static const uint8_t PIN_LEDS = 0;         // D3 -> 2 x WS2812B chaînées
static const uint8_t PIN_BUTTON = 2;       // D4 -> bouton (actif bas, pull-up sur la WeMos)
static const uint8_t LED_COUNT = 2;
// Sonde DS18B20 optionnelle : broche SDA du connecteur I2C du shield (pull-up
// 4,7 kΩ à souder sur l'emplacement R1).
static const uint8_t PIN_TEMP = 4;         // D2 -> DQ du DS18B20
static const uint32_t TEMP_READ_INTERVAL_MS = 30000;

// --- Valeurs par défaut (modifiables depuis l'interface web) --------------
// Le poêle peut fonctionner dans la bande 433 MHz (télécommande d'origine à
// 434,50 MHz selon le manuel MCZ) ou 868 MHz (868,95 MHz, mesuré). Le module
// radio doit être de la version correspondante (RFM69 433 ou 868).
static const uint32_t FREQ_433_HZ = 434500000;
static const uint32_t FREQ_868_HZ = 868950000;
static const uint32_t DEFAULT_FREQUENCY_HZ = FREQ_868_HZ;
static const uint16_t DEFAULT_CHIP_US = 414;           // demi-bit Manchester mesuré sur la télécommande
static const bool DEFAULT_HIGH_POWER = true;           // RFM69HW (validé sur le poêle)
static const int8_t DEFAULT_POWER_DBM = 13;            // 868,7-869,2 MHz : 25 mW (14 dBm) max
                                                       // 433,05-434,79 MHz : 10 mW (10 dBm) max
static const uint8_t DEFAULT_REPEATS = 5;
static const uint16_t DEFAULT_RESEND_S = 300;          // renvoi silencieux, comme la télécommande (0 = jamais)
static const uint16_t DEFAULT_MQTT_PORT = 1883;

// --- Mode autonome -----------------------------------------------------------
// Fuseau horaire POSIX (France : heure d'été du dernier dimanche de mars au
// dernier dimanche d'octobre) et serveur de temps.
static const char* const DEFAULT_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char* const DEFAULT_NTP = "pool.ntp.org";
static const uint32_t AUTO_INTERVAL_MS = 5000;         // période de la régulation

// --- Réseau ----------------------------------------------------------------
static const char* const AP_PASSWORD = "mczconfig";    // mot de passe du point d'accès de configuration
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
static const uint32_t WIFI_RETRY_INTERVAL_MS = 60000;  // en mode AP, nouvel essai de connexion
static const uint32_t BUTTON_RESET_MS = 8000;          // appui long : effacement du WiFi

// --- Fichiers LittleFS ------------------------------------------------------
static const char* const SETTINGS_FILE = "/config.json";
static const char* const STATE_FILE = "/state.json";
static const char* const SCHEDULE_FILE = "/schedule.json";  // programmation + mode autonome actif
