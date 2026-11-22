#include "Thermostat.h"

namespace thermo {

void Week::clear() {
  for (auto& d : days) d.count = 0;
}

bool Week::add(uint8_t day, Slot s) {
  if (day >= DAYS || s.start >= s.end || s.end > 1440) return false;
  if (s.target != TARGET_OFF && (s.target < TARGET_MIN || s.target > TARGET_MAX)) return false;
  Day& d = days[day];
  if (d.count >= MAX_SLOTS) return false;
  uint8_t pos = 0;
  while (pos < d.count && d.slots[pos].start < s.start) pos++;
  if (pos > 0 && d.slots[pos - 1].end > s.start) return false;
  if (pos < d.count && d.slots[pos].start < s.end) return false;
  for (uint8_t i = d.count; i > pos; i--) d.slots[i] = d.slots[i - 1];
  d.slots[pos] = s;
  d.count++;
  return true;
}

int16_t Week::targetAt(uint8_t day, uint16_t minute) const {
  if (day >= DAYS) return TARGET_OFF;
  const Day& d = days[day];
  for (uint8_t i = 0; i < d.count; i++)
    if (minute >= d.slots[i].start && minute < d.slots[i].end) return d.slots[i].target;
  return TARGET_OFF;
}

bool Week::nextChange(uint8_t day, uint16_t minute, uint8_t& outDay, uint16_t& outMinute,
                      int16_t& outTarget) const {
  const int16_t current = targetAt(day, minute);
  // Les changements ne peuvent avoir lieu qu'aux bornes des plages : on les
  // parcourt dans l'ordre sur 7 jours + le jour courant.
  for (uint8_t k = 0; k <= DAYS; k++) {
    uint8_t dd = (day + k) % DAYS;
    const Day& d = days[dd];
    // bornes candidates de la journée, triées : minuit, débuts et fins
    uint16_t marks[2 * MAX_SLOTS + 1];
    uint8_t n = 0;
    marks[n++] = 0;
    for (uint8_t i = 0; i < d.count; i++) {
      marks[n++] = d.slots[i].start;
      if (d.slots[i].end < 1440) marks[n++] = d.slots[i].end;
    }
    for (uint8_t i = 1; i < n; i++)  // tri par insertion (n petit)
      for (uint8_t j = i; j > 0 && marks[j] < marks[j - 1]; j--) {
        uint16_t t = marks[j];
        marks[j] = marks[j - 1];
        marks[j - 1] = t;
      }
    for (uint8_t i = 0; i < n; i++) {
      if (k == 0 && marks[i] <= minute) continue;
      int16_t t = targetAt(dd, marks[i]);
      if (t != current) {
        outDay = dd;
        outMinute = marks[i];
        outTarget = t;
        return true;
      }
    }
  }
  return false;
}

const char* phaseName(Phase p) {
  switch (p) {
    case Phase::Startup: return "startup";
    case Phase::Heat: return "heat";
    case Phase::Hold: return "hold";
    default: return "off";
  }
}

void Regulator::enter(Phase ph, uint32_t nowS, int16_t temp) {
  _phase = ph;
  _phaseStart = nowS;
  if (ph == Phase::Hold) _holdRef = temp;
}

void Regulator::reset(bool stoveOn, uint32_t nowS) {
  enter(stoveOn ? Phase::Heat : Phase::Off, nowS);
  _lastValidTemp = nowS;
}

uint8_t Regulator::power(const Params& p) const {
  switch (_phase) {
    case Phase::Startup: return p.startupPower;
    case Phase::Heat: return p.heatPower;
    case Phase::Hold: return p.holdPower;
    default: return 0;
  }
}

uint32_t Regulator::phaseDuration(const Params& p) const {
  switch (_phase) {
    case Phase::Startup: return p.startupMin * 60UL;
    case Phase::Hold: return p.holdMin * 60UL;
    default: return 0;
  }
}

void Regulator::update(const Params& p, uint32_t nowS, int16_t target, bool tempValid, int16_t temp,
                       bool allowStart) {
  if (target == TARGET_OFF) {
    if (_phase != Phase::Off) enter(Phase::Off, nowS);
    return;
  }
  if (!tempValid) {
    // Sans mesure, on ne sait pas réguler : on garde la phase en cours un
    // moment (sonde qui rate une lecture), puis on éteint.
    if (_phase != Phase::Off && nowS - _lastValidTemp > p.sensorTimeoutMin * 60UL) enter(Phase::Off, nowS);
    return;
  }
  _lastValidTemp = nowS;

  const uint32_t elapsed = nowS - _phaseStart;
  switch (_phase) {
    case Phase::Off:
      if (allowStart && temp <= target - p.hysteresis) enter(Phase::Startup, nowS);
      break;
    case Phase::Startup:
      if (elapsed >= p.startupMin * 60UL) {
        if (temp >= target) enter(Phase::Hold, nowS, temp);
        else enter(Phase::Heat, nowS);
      }
      break;
    case Phase::Heat:
      if (temp >= target) enter(Phase::Hold, nowS, temp);
      break;
    case Phase::Hold:
      if (elapsed >= p.holdMin * 60UL) {
        if (temp < _holdRef - p.dropTolerance) enter(Phase::Heat, nowS);
        else enter(Phase::Off, nowS);
      }
      break;
  }
}

}  // namespace thermo
