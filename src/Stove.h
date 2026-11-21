// État du poêle et envoi des commandes radio.
//
// La liaison radio est unidirectionnelle : l'état présenté est le dernier état
// commandé (mode "optimiste"), mémorisé en flash pour survivre aux redémarrages.
// Les commandes reçues (web, MQTT, bouton) ne font qu'enregistrer une demande ;
// l'émission radio, bloquante (~0,5 s), est faite dans loop().
//
// Seuls marche/arrêt, puissance et ventilation agissent sur le poêle : le champ
// "mode" de la trame (manuel/auto/eco) est ignoré par le poêle.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MczProtocol.h>
#include <Rfm69Tx.h>

#include <functional>
#include <vector>

struct StoveState {
  bool on = false;
  uint8_t power = 3;            // 1..5
  uint8_t fan = mcz::FAN_AUTO;  // 1..5, 6 = auto
};

class Stove {
 public:
  using Listener = std::function<void()>;

  explicit Stove(Rfm69Tx& radio) : _radio(radio) {}

  void begin();  // charge l'état et applique les réglages radio
  void loop();
  void applyRadioSettings();

  const StoveState& state() const { return _state; }
  bool isOn() const { return _state.on; }

  // Commandes (émission avec bip) ; retournent false si la valeur est invalide.
  void setOn(bool on);
  bool setPower(uint8_t p);
  bool setFan(uint8_t f);
  void resend() { request(true); }

  // Applique {"on":true,"power":3,"fan":"auto"} (champs facultatifs).
  bool applyJson(JsonObjectConst obj, String& error);
  void toJson(JsonObject obj) const;

  void onChange(Listener l) { _listeners.push_back(l); }

  uint32_t lastTxMillis() const { return _lastTx; }
  bool lastTxOk() const { return _lastTxOk; }
  uint32_t txCount() const { return _txCount; }

 private:
  void request(bool beep);
  void changed();
  void send(bool beep);
  void saveState();
  bool loadState();

  Rfm69Tx& _radio;
  StoveState _state;
  bool _pending = false;
  bool _pendingBeep = false;
  bool _dirty = false;
  uint32_t _lastTx = 0;
  bool _lastTxOk = false;
  uint32_t _txCount = 0;
  std::vector<Listener> _listeners;
};
