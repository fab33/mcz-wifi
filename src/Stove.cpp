#include "Stove.h"

#include <LittleFS.h>

#include "Settings.h"
#include "config.h"

void Stove::begin() {
  loadState();
  applyRadioSettings();
  // Pas d'émission au démarrage : un redémarrage du module (coupure de
  // courant, mise à jour) ne doit pas modifier l'état du poêle. Le renvoi
  // périodique ne commence qu'après la première commande.
}

void Stove::applyRadioSettings() {
  if (!_radio.present()) return;
  _radio.setFrequency(settings.radio.frequencyHz);
  _radio.setChipDuration(settings.radio.chipUs);
  _radio.setModule(settings.radio.highPower ? Rfm69Tx::Module::HW : Rfm69Tx::Module::W);
  _radio.setPowerDbm(settings.radio.powerDbm);
}

void Stove::request(bool beep) {
  _pending = true;
  _pendingBeep |= beep;
}

void Stove::loop() {
  if (_dirty) {
    saveState();
    _dirty = false;
  }
  // Renvoi périodique silencieux, comme la télécommande d'origine.
  uint32_t period = (uint32_t)settings.stove.resendSeconds * 1000UL;
  if (period > 0 && _txCount > 0 && millis() - _lastTx >= period) request(false);
  if (_pending) {
    bool beep = _pendingBeep;
    _pending = _pendingBeep = false;
    send(beep);
  }
}

void Stove::send(bool beep) {
  mcz::Command cmd;
  cmd.id = settings.stove.id;
  cmd.mode = _state.on ? mcz::MODE_ON : mcz::Mode::Off;
  // Bip seulement à la mise en route et à l'arrêt : jamais pour un changement
  // de puissance ou de ventilation, ni pour un renvoi du même état.
  beep = beep && _state.on != _sent.on;
  // Arrêt : puissance et ventilation de la trame précédente (voir Stove.h).
  if (_state.on) {
    _sent.power = _state.power;
    _sent.fan = _state.fan;
    _dirty = true;  // _sent est enregistré avec l'état
  }
  _sent.on = _state.on;
  cmd.power = _sent.power;
  cmd.fan1 = _sent.fan;
  cmd.fan2 = settings.stove.fan2;
  cmd.beep = beep;

  mcz::Timing t;
  t.repeats = settings.radio.repeats;
  uint8_t buf[255];
  size_t n = mcz::buildChipStream(cmd, t, buf, sizeof(buf));

  _lastTxOk = n > 0 && _radio.transmit(buf, n);
  _lastTx = millis();
  _txCount++;
  Serial.printf("[RF] %s puissance=%u ventilo=%u%s -> %s\n", _state.on ? "marche" : "arret", cmd.power,
                cmd.fan1, beep ? "" : " (silencieux)", _lastTxOk ? "OK" : "ECHEC");
  for (auto& l : _listeners) l();  // publie aussi les infos d'émission
}

void Stove::changed(bool manual, bool beep, bool transmit) {
  _dirty = true;
  if (transmit) request(beep);
  if (manual)
    for (auto& l : _manualListeners) l();
  for (auto& l : _listeners) l();
}

void Stove::applyAutomatic(const StoveState& s, bool beep) {
  const bool transmit = s.on || _state.on;
  _state = s;
  changed(false, beep, transmit);
}

void Stove::setOn(bool on) {
  _state.on = on;
  changed();
}

bool Stove::setPower(uint8_t p) {
  if (p < mcz::POWER_MIN || p > mcz::POWER_MAX) return false;
  _state.power = p;
  changed(true, true, _state.on);
  return true;
}

bool Stove::setFan(uint8_t f) {
  if (f < 1 || f > mcz::FAN_AUTO) return false;
  _state.fan = f;
  changed(true, true, _state.on);
  return true;
}

bool Stove::applyJson(JsonObjectConst obj, String& error) {
  StoveState next = _state;
  if (!obj["on"].isNull()) {
    if (!obj["on"].is<bool>()) {
      error = "\"on\" doit valoir true ou false";
      return false;
    }
    next.on = obj["on"].as<bool>();
  }
  if (!obj["power"].isNull()) {
    int p = obj["power"] | -1;
    if (p < mcz::POWER_MIN || p > mcz::POWER_MAX) {
      error = "puissance invalide (1 à 5)";
      return false;
    }
    next.power = p;
  }
  if (!obj["fan"].isNull()) {
    int f = obj["fan"].is<const char*>() && strcmp(obj["fan"], "auto") == 0 ? mcz::FAN_AUTO
                                                                               : (obj["fan"] | -1);
    if (f < 1 || f > mcz::FAN_AUTO) {
      error = "ventilation invalide (1 à 5 ou auto)";
      return false;
    }
    next.fan = f;
  }
  // Poêle arrêté et marche/arrêt non demandé : réglage seulement mémorisé.
  const bool transmit = next.on || !obj["on"].isNull();
  _state = next;
  changed(true, true, transmit);
  return true;
}

void Stove::toJson(JsonObject obj) const {
  obj["on"] = _state.on;
  obj["power"] = _state.power;
  if (_state.fan == mcz::FAN_AUTO) obj["fan"] = "auto";
  else obj["fan"] = _state.fan;
  obj["tx_count"] = _txCount;
  obj["last_tx_ok"] = _lastTxOk;
  obj["last_tx_age_s"] = _txCount ? (long)((millis() - _lastTx) / 1000) : -1;
}

void Stove::saveState() {
  JsonDocument doc;
  doc["on"] = _state.on;
  doc["power"] = _state.power;
  doc["fan"] = _state.fan;
  doc["sent_power"] = _sent.power;
  doc["sent_fan"] = _sent.fan;
  File f = LittleFS.open(STATE_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

bool Stove::loadState() {
  File f = LittleFS.open(STATE_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  bool ok = !deserializeJson(doc, f);
  f.close();
  if (!ok) return false;
  // compatibilité avec l'ancien format {"mode":3,...}
  _state.on = doc["on"].is<bool>() ? doc["on"].as<bool>() : (doc["mode"] | 0) != 0;
  uint8_t p = doc["power"] | 3;
  uint8_t fan = doc["fan"] | mcz::FAN_AUTO;
  _state.power = constrain(p, mcz::POWER_MIN, mcz::POWER_MAX);
  _state.fan = constrain(fan, (uint8_t)1, mcz::FAN_AUTO);
  _sent = _state;
  uint8_t sp = doc["sent_power"] | _state.power;
  uint8_t sf = doc["sent_fan"] | _state.fan;
  _sent.power = constrain(sp, mcz::POWER_MIN, mcz::POWER_MAX);
  _sent.fan = constrain(sf, (uint8_t)1, mcz::FAN_AUTO);
  return true;
}
