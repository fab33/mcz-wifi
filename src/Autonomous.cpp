#include "Autonomous.h"

#include <LittleFS.h>
#include <MczProtocol.h>

#include "Clock.h"
#include "Settings.h"
#include "Stove.h"
#include "TempSensor.h"
#include "config.h"

Autonomous autonomous;

static int16_t toTenths(float c) { return (int16_t)lroundf(c * 10.0f); }

void Autonomous::begin(Stove& stove, TempSensor& temp) {
  _stove = &stove;
  _temp = &temp;
  _week.clear();
  if (!load()) Serial.println("[Auto] Aucune programmation");
  _reg.reset(_stove->isOn(), sysClock.uptimeS());
  // Une commande manuelle reprend la main.
  _stove->onManualChange([this]() {
    if (_enabled) {
      Serial.println("[Auto] Commande manuelle : mode autonome désactivé");
      setEnabled(false);
    }
  });
}

thermo::Params Autonomous::params() const {
  thermo::Params p;
  const AutoSettings& a = settings.autoMode;
  p.hysteresis = toTenths(a.hysteresis);
  p.startupPower = a.startupPower;
  p.startupMin = a.startupMin;
  p.heatPower = a.heatPower;
  p.holdPower = a.holdPower;
  p.holdMin = a.holdMin;
  return p;
}

void Autonomous::setEnabled(bool on) {
  if (on == _enabled) return;
  _enabled = on;
  if (on) {
    _reg.reset(_stove->isOn(), sysClock.uptimeS());
    _lastStep = 0;  // régulation immédiate
    _syncPending = true;
  }
  save();
  Serial.printf("[Auto] Mode autonome %s\n", on ? "activé" : "désactivé");
  notify();
}

void Autonomous::loop() {
  if (!_enabled) return;
  if (_lastStep && millis() - _lastStep < AUTO_INTERVAL_MS) return;
  _lastStep = millis();
  step();
}

void Autonomous::step() {
  uint8_t day;
  uint16_t minute;
  bool timeValid = sysClock.local(day, minute);
  if (timeValid != _timeValid) {
    _timeValid = timeValid;
    notify();
  }
  int16_t target;
  if (timeValid) {
    target = _week.targetAt(day, minute);
    if (!_hasLastTarget || target != _lastTarget) {
      _hasLastTarget = true;
      _lastTarget = target;
      save();  // pour le mode dégradé après un redémarrage sans heure
    }
  } else if (_hasLastTarget) {
    target = _lastTarget;  // mode dégradé : pas de nouvel allumage
  } else {
    return;  // ni heure ni consigne connue : on ne touche à rien
  }

  const thermo::Phase before = _reg.phase();
  const bool tempValid = _temp->valid();
  _reg.update(params(), sysClock.uptimeS(), target, tempValid, tempValid ? toTenths(_temp->celsius()) : 0,
              timeValid);
  apply();
  if (target != _target || _reg.phase() != before) {
    if (_reg.phase() != before)
      Serial.printf("[Auto] Phase %s -> %s\n", thermo::phaseName(before), thermo::phaseName(_reg.phase()));
    _target = target;
    notify();
  }
}

// Envoie au poêle l'état voulu par la régulation, seulement s'il diffère.
void Autonomous::apply() {
  const StoveState& cur = _stove->state();
  StoveState next = cur;
  next.on = _reg.on();
  if (next.on) {
    next.power = _reg.power(params());
    next.fan = mcz::FAN_AUTO;
  }
  if (next.on != cur.on || next.power != cur.power || next.fan != cur.fan)
    _stove->applyAutomatic(next, settings.autoMode.beep);
  else if (_syncPending)
    _stove->resendSilent();
  _syncPending = false;
  // Ensuite, l'état est renvoyé sans bip toutes les stove.resend_s secondes
  // (300 s par défaut) par Stove::loop(), comme pour les commandes manuelles.
}

void Autonomous::notify() {
  for (auto& l : _listeners) l();
}

// --- programmation ------------------------------------------------------------

void Autonomous::scheduleToJson(JsonObject obj) const {
  JsonArray days = obj["days"].to<JsonArray>();
  for (uint8_t d = 0; d < thermo::DAYS; d++) {
    JsonArray slots = days.add<JsonArray>();
    const thermo::Day& day = _week.days[d];
    for (uint8_t i = 0; i < day.count; i++) {
      JsonObject s = slots.add<JsonObject>();
      s["start"] = day.slots[i].start;
      s["end"] = day.slots[i].end;
      if (day.slots[i].target == thermo::TARGET_OFF) s["target"] = nullptr;
      else s["target"] = day.slots[i].target / 10.0f;
    }
  }
}

static bool parseWeek(JsonObjectConst obj, thermo::Week& w, String& error) {
  static const char* const NAMES[] = {"lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"};
  JsonArrayConst days = obj["days"];
  if (days.isNull() || days.size() != thermo::DAYS) {
    error = "\"days\" doit contenir 7 journées";
    return false;
  }
  w.clear();
  for (uint8_t d = 0; d < thermo::DAYS; d++) {
    for (JsonObjectConst s : days[d].as<JsonArrayConst>()) {
      thermo::Slot slot;
      int start = s["start"] | -1, end = s["end"] | -1;
      slot.start = start < 0 ? 0 : start;
      slot.end = end < 0 ? 0 : end;
      JsonVariantConst t = s["target"];
      slot.target = t.isNull() ? thermo::TARGET_OFF : toTenths(t.as<float>());
      if (start < 0 || end < 0 || !w.add(d, slot)) {
        error = String("plage invalide ou qui en chevauche une autre (") + NAMES[d] + ")";
        return false;
      }
    }
  }
  return true;
}

bool Autonomous::scheduleFromJson(JsonObjectConst obj, String& error) {
  thermo::Week w;
  if (!parseWeek(obj, w, error)) return false;
  _week = w;
  _lastStep = 0;  // prise en compte immédiate
  if (!save()) {
    error = "écriture impossible";
    return false;
  }
  notify();
  return true;
}

bool Autonomous::load() {
  File f = LittleFS.open(SCHEDULE_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  String error;
  JsonObjectConst root = doc.as<JsonObjectConst>();
  thermo::Week w;
  if (parseWeek(root, w, error)) _week = w;
  else Serial.printf("[Auto] Programmation ignorée : %s\n", error.c_str());
  _enabled = root["enabled"] | false;
  JsonVariantConst lt = root["last_target"];
  if (!lt.isUnbound()) {
    _hasLastTarget = true;
    _lastTarget = lt.isNull() ? thermo::TARGET_OFF : toTenths(lt.as<float>());
  }
  return true;
}

bool Autonomous::save() const {
  JsonDocument doc;
  scheduleToJson(doc.to<JsonObject>());
  doc["enabled"] = _enabled;
  if (_hasLastTarget) {
    if (_lastTarget == thermo::TARGET_OFF) doc["last_target"] = nullptr;
    else doc["last_target"] = _lastTarget / 10.0f;
  }
  File f = LittleFS.open(SCHEDULE_FILE, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

// --- état -----------------------------------------------------------------------

void Autonomous::statusToJson(JsonObject obj) {
  obj["enabled"] = _enabled;
  obj["time"] = sysClock.iso();
  uint8_t day;
  uint16_t minute;
  bool timeValid = sysClock.local(day, minute);
  obj["time_valid"] = timeValid;
  obj["sensor"] = _temp->valid();
  const bool degraded = !timeValid && _hasLastTarget;
  obj["degraded"] = degraded;
  if (!timeValid && !degraded) return;

  const int16_t target = timeValid ? _week.targetAt(day, minute) : _lastTarget;
  if (target == thermo::TARGET_OFF) obj["target"] = nullptr;
  else obj["target"] = target / 10.0f;

  if (timeValid) {
    obj["day"] = day;
    obj["minute"] = minute;
    uint8_t nd;
    uint16_t nm;
    int16_t nt;
    if (_week.nextChange(day, minute, nd, nm, nt)) {
      JsonObject n = obj["next"].to<JsonObject>();
      n["day"] = nd;
      n["minute"] = nm;
      if (nt == thermo::TARGET_OFF) n["target"] = nullptr;
      else n["target"] = nt / 10.0f;
    }
  }
  if (!_enabled) return;
  const thermo::Params p = params();
  const uint32_t now = sysClock.uptimeS();
  obj["phase"] = thermo::phaseName(_reg.phase());
  if (_reg.on()) obj["power"] = _reg.power(p);
  obj["phase_elapsed_s"] = _reg.phaseElapsed(now);
  obj["phase_duration_s"] = _reg.phaseDuration(p);
  if (_reg.phase() == thermo::Phase::Hold) obj["hold_ref"] = _reg.holdReference() / 10.0f;
}
