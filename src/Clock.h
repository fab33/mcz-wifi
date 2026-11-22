// Date et heure locales, synchronisées par NTP (nécessaires au mode autonome).
//
// Le fuseau horaire (règles d'heure d'été comprises) et le serveur NTP se
// règlent dans Réglages › Mode autonome. Tant que l'heure n'a pas été reçue,
// valid() est faux et le mode autonome n'agit pas sur le poêle.
#pragma once

#include <Arduino.h>

class Clock {
 public:
  void begin();        // (re)configure le fuseau et le serveur NTP
  bool valid() const;
  // Jour (0 = lundi .. 6 = dimanche) et minute de la journée (0..1439).
  bool local(uint8_t& day, uint16_t& minute) const;
  String iso() const;  // "2026-11-22T14:32:05" (heure locale), vide si inconnue
  // Horloge monotone en secondes, insensible au retour à zéro de millis().
  uint32_t uptimeS();

 private:
  uint32_t _lastMs = 0;
  uint32_t _secs = 0;
  uint32_t _remMs = 0;
};

extern Clock sysClock;
