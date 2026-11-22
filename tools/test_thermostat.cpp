// Test hors-cible de la bibliothèque Thermostat (mode autonome).
//
//   g++ -std=c++17 -I lib/Thermostat lib/Thermostat/Thermostat.cpp tools/test_thermostat.cpp -o /tmp/test_thermo && /tmp/test_thermo
#include <cstdio>

#include "Thermostat.h"

using namespace thermo;

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      printf("ÉCHEC %s:%d : %s\n", __FILE__, __LINE__, #cond);   \
      failures++;                                                \
    }                                                            \
  } while (0)

static void testWeek() {
  Week w;
  w.clear();
  CHECK(w.add(0, {360, 480, 200}));      // lun 6:00-8:00 20,0
  CHECK(w.add(0, {1080, 1440, 210}));    // lun 18:00-24:00 21,0
  CHECK(w.add(0, {480, 1080, TARGET_OFF}));
  CHECK(!w.add(0, {470, 500, 190}));     // chevauchement
  CHECK(!w.add(0, {500, 500, 190}));     // vide
  CHECK(!w.add(0, {0, 100, 400}));       // consigne hors limites
  CHECK(!w.add(7, {0, 100, 200}));       // jour invalide
  CHECK(w.days[0].count == 3);
  CHECK(w.days[0].slots[1].start == 480);  // tri

  CHECK(w.targetAt(0, 359) == TARGET_OFF);
  CHECK(w.targetAt(0, 360) == 200);
  CHECK(w.targetAt(0, 479) == 200);
  CHECK(w.targetAt(0, 480) == TARGET_OFF);
  CHECK(w.targetAt(0, 1439) == 210);
  CHECK(w.targetAt(1, 600) == TARGET_OFF);

  uint8_t d;
  uint16_t m;
  int16_t t;
  CHECK(w.nextChange(0, 400, d, m, t) && d == 0 && m == 480 && t == TARGET_OFF);
  CHECK(w.nextChange(0, 500, d, m, t) && d == 0 && m == 1080 && t == 210);
  CHECK(w.nextChange(0, 1200, d, m, t) && d == 1 && m == 0 && t == TARGET_OFF);
  // dimanche soir -> lundi 6:00 (retour sur la semaine)
  CHECK(w.nextChange(6, 1200, d, m, t) && d == 0 && m == 360 && t == 200);

  Week empty;
  empty.clear();
  CHECK(!empty.nextChange(3, 100, d, m, t));
}

static void testRegulator() {
  Params p;
  Regulator r;
  uint32_t now = 1000;
  r.reset(false, now);
  CHECK(r.phase() == Phase::Off);

  // 19,5 °C pour une consigne de 20 : pas d'allumage (écart 1 °C)
  r.update(p, now, 200, true, 195);
  CHECK(r.phase() == Phase::Off);
  // 19,0 : allumage puissance 5
  r.update(p, now, 200, true, 190);
  CHECK(r.phase() == Phase::Startup && r.power(p) == 5 && r.on());
  // consigne atteinte pendant l'allumage : on reste 20 min en puissance 5
  now += 10 * 60;
  r.update(p, now, 200, true, 201);
  CHECK(r.phase() == Phase::Startup);
  now += 10 * 60;
  r.update(p, now, 200, true, 201);
  CHECK(r.phase() == Phase::Hold && r.power(p) == 1 && r.holdReference() == 201);
  // maintien 15 min, température stable -> arrêt
  now += 14 * 60;
  r.update(p, now, 200, true, 201);
  CHECK(r.phase() == Phase::Hold);
  now += 60;
  r.update(p, now, 200, true, 200);  // -0,1 : dans la tolérance
  CHECK(r.phase() == Phase::Off && !r.on());

  // nouvel allumage, consigne non atteinte après 20 min -> chauffe puissance 3
  r.update(p, now, 200, true, 190);
  CHECK(r.phase() == Phase::Startup);
  now += 20 * 60;
  r.update(p, now, 200, true, 195);
  CHECK(r.phase() == Phase::Heat && r.power(p) == 3);
  r.update(p, now, 200, true, 199);
  CHECK(r.phase() == Phase::Heat);
  r.update(p, now, 200, true, 200);
  CHECK(r.phase() == Phase::Hold);
  // la température baisse pendant le maintien -> retour en chauffe
  now += 15 * 60;
  r.update(p, now, 200, true, 197);
  CHECK(r.phase() == Phase::Heat);

  // plage « arrêt » : extinction immédiate
  r.update(p, now, TARGET_OFF, true, 197);
  CHECK(r.phase() == Phase::Off);

  // perte de la sonde : on tient 5 min puis on éteint
  r.update(p, now, 200, true, 180);
  CHECK(r.phase() == Phase::Startup);
  now += 4 * 60;
  r.update(p, now, 200, false, 0);
  CHECK(r.phase() == Phase::Startup);
  now += 2 * 60;
  r.update(p, now, 200, false, 0);
  CHECK(r.phase() == Phase::Off);
  // sans sonde, pas d'allumage
  r.update(p, now + 600, 200, false, 0);
  CHECK(r.phase() == Phase::Off);

  // reprise avec le poêle déjà allumé : pas d'extinction, passage en chauffe
  r.reset(true, now);
  CHECK(r.phase() == Phase::Heat);
  r.update(p, now, 200, true, 195);
  CHECK(r.phase() == Phase::Heat);
}

// Mode dégradé (heure inconnue) : régulation sur la dernière consigne, sans allumage.
static void testDegraded() {
  Params p;
  Regulator r;
  uint32_t now = 0;
  r.reset(true, now);  // redémarrage du module, poêle allumé
  r.update(p, now, 200, true, 195, false);
  CHECK(r.phase() == Phase::Heat);
  r.update(p, now, 200, true, 200, false);
  CHECK(r.phase() == Phase::Hold);
  now += 15 * 60;
  r.update(p, now, 200, true, 200, false);
  CHECK(r.phase() == Phase::Off);
  // la pièce refroidit : pas de rallumage sans l'heure
  r.update(p, now + 3600, 200, true, 170, false);
  CHECK(r.phase() == Phase::Off);
  // retour de l'heure : régulation normale
  r.update(p, now + 3600, 200, true, 170, true);
  CHECK(r.phase() == Phase::Startup);
  // dernière consigne « arrêt » : extinction
  r.reset(true, now);
  r.update(p, now, TARGET_OFF, true, 180, false);
  CHECK(r.phase() == Phase::Off);
}

int main() {
  testWeek();
  testRegulator();
  testDegraded();
  if (failures) {
    printf("%d échec(s)\n", failures);
    return 1;
  }
  printf("Tous les tests passent\n");
  return 0;
}
