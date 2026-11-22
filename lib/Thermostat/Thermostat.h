// Mode autonome : programmation hebdomadaire et régulation de température.
//
// Bibliothèque C++ pure (aucune dépendance Arduino), testée sur PC
// (tools/test_thermostat.cpp).
//
// Températures en dixièmes de degré (205 = 20,5 °C), heures en minutes depuis
// minuit (0..1440), jours numérotés 0 = lundi .. 6 = dimanche.
//
// Programmation : chaque jour a ses propres plages [début, fin[, chacune avec
// une consigne ou « arrêt ». En dehors de toute plage, le poêle est arrêté.
//
// Régulation (paramètres par défaut entre parenthèses) :
//   ARRÊT     poêle éteint ; allumage si T <= consigne - écart (1 °C)
//   ALLUMAGE  puissance 5 pendant 20 min, puis MAINTIEN si la consigne est
//             atteinte, sinon CHAUFFE
//   CHAUFFE   puissance 3 jusqu'à la consigne, puis MAINTIEN
//   MAINTIEN  puissance 1 pendant 15 min, puis : si la température a baissé
//             depuis le début du maintien -> CHAUFFE, sinon -> ARRÊT
// Une plage « arrêt » (ou l'absence de plage) éteint le poêle immédiatement.
// Sans mesure de température pendant plus de 5 min, le poêle est éteint.
//
// Mode dégradé (allowStart = false, utilisé quand l'heure est inconnue) : la
// régulation continue sur la dernière consigne connue mais n'allume jamais le
// poêle ; une fois revenu à l'ARRÊT, il y reste.
#pragma once

#include <stdint.h>
#include <limits.h>

namespace thermo {

static const int16_t TARGET_OFF = INT16_MIN;  // plage « arrêt » / pas de plage
static const uint8_t DAYS = 7;
static const uint8_t MAX_SLOTS = 12;          // plages par jour
static const int16_t TARGET_MIN = 50;         // 5,0 °C
static const int16_t TARGET_MAX = 300;        // 30,0 °C

struct Slot {
  uint16_t start;  // minutes, inclus
  uint16_t end;    // minutes, exclu (1440 = minuit en fin de journée)
  int16_t target;  // dixièmes de °C, ou TARGET_OFF
};

struct Day {
  Slot slots[MAX_SLOTS];
  uint8_t count = 0;
};

struct Week {
  Day days[DAYS];

  void clear();
  // Ajoute une plage (insertion triée). Retourne false si elle est invalide ou
  // chevauche une plage existante, ou si la journée est pleine.
  bool add(uint8_t day, Slot s);
  // Consigne à l'instant donné (TARGET_OFF si aucune plage).
  int16_t targetAt(uint8_t day, uint16_t minute) const;
  // Prochain changement de consigne après l'instant donné (sur 7 jours).
  // Retourne false si la consigne ne change jamais.
  bool nextChange(uint8_t day, uint16_t minute, uint8_t& outDay, uint16_t& outMinute,
                  int16_t& outTarget) const;
};

struct Params {
  int16_t hysteresis = 10;     // allumage à consigne - 1,0 °C
  uint8_t startupPower = 5;
  uint16_t startupMin = 20;
  uint8_t heatPower = 3;
  uint8_t holdPower = 1;
  uint16_t holdMin = 15;
  int16_t dropTolerance = 1;   // baisse ignorée pendant le maintien (bruit de mesure, 0,1 °C)
  uint16_t sensorTimeoutMin = 5;
};

enum class Phase : uint8_t { Off, Startup, Heat, Hold };
const char* phaseName(Phase p);  // "off", "startup", "heat", "hold"

class Regulator {
 public:
  // stoveOn : état actuel du poêle. S'il est déjà allumé, la régulation
  // reprend en CHAUFFE au lieu de l'éteindre.
  void reset(bool stoveOn, uint32_t nowS);

  // À appeler régulièrement (toutes les quelques secondes).
  // nowS : horloge monotone en secondes ; temp en dixièmes de °C.
  void update(const Params& p, uint32_t nowS, int16_t target, bool tempValid, int16_t temp,
              bool allowStart = true);

  Phase phase() const { return _phase; }
  bool on() const { return _phase != Phase::Off; }
  uint8_t power(const Params& p) const;
  uint32_t phaseElapsed(uint32_t nowS) const { return nowS - _phaseStart; }
  // Durée prévue de la phase en secondes (0 = jusqu'à un changement de température).
  uint32_t phaseDuration(const Params& p) const;
  int16_t holdReference() const { return _holdRef; }

 private:
  void enter(Phase ph, uint32_t nowS, int16_t temp = 0);

  Phase _phase = Phase::Off;
  uint32_t _phaseStart = 0;
  int16_t _holdRef = 0;
  uint32_t _lastValidTemp = 0;
};

}  // namespace thermo
