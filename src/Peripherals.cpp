#include "Peripherals.h"

#include <Rfm69Tx.h>

#include "HomeAssistant.h"
#include "Network.h"
#include "Settings.h"
#include "Stove.h"
#include "config.h"

static const uint8_t BRIGHTNESS = 40;  // les WS2812 sont très lumineuses
static const uint32_t DEBOUNCE_MS = 50;

Peripherals::Peripherals(Stove& stove, HomeAssistant& ha, Rfm69Tx& radio)
    : _stove(stove), _ha(ha), _radio(radio), _leds(LED_COUNT, PIN_LEDS, NEO_GRB + NEO_KHZ800) {}

void Peripherals::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  _leds.begin();
  _leds.setBrightness(BRIGHTNESS);
  _leds.clear();
  _leds.show();
}

void Peripherals::loop() {
  handleButton();
  if (millis() - _lastLedUpdate > 100) {
    _lastLedUpdate = millis();
    updateLeds();
  }
}

void Peripherals::handleButton() {
  bool pressed = digitalRead(PIN_BUTTON) == LOW;
  uint32_t now = millis();
  if (pressed && !_lastButton) _pressedSince = now;

  if (pressed && _pressedSince && now - _pressedSince > BUTTON_RESET_MS) {
    Serial.println("[Bouton] Appui long : effacement du WiFi");
    _leds.fill(_leds.Color(160, 0, 255));
    _leds.show();
    settings.clearWifi();
    delay(500);
    ESP.restart();
  }
  if (!pressed && _lastButton && _pressedSince && now - _pressedSince > DEBOUNCE_MS &&
      now - _pressedSince < 2000) {
    Serial.println("[Bouton] Marche/arrêt");
    _stove.setOn(!_stove.isOn());
  }
  _lastButton = pressed;
}

void Peripherals::updateLeds() {
  uint32_t now = millis();
  bool blink = (now / 500) % 2;

  uint32_t net;
  if (!_radio.present()) net = _leds.Color(255, 0, 0);
  else if (network.apActive() && !network.connected()) net = blink ? _leds.Color(0, 0, 255) : 0;
  else if (!network.connected()) net = _leds.Color(255, 0, 0);
  else if (settings.mqtt.enabled && !_ha.connected()) net = _leds.Color(255, 100, 0);
  else net = _leds.Color(0, 255, 0);

  // appui long en cours : prévient l'utilisateur avant l'effacement
  if (_lastButton && now - _pressedSince > 2000) net = blink ? _leds.Color(160, 0, 255) : 0;

  if (_stove.txCount() != _lastTxSeen) {
    _lastTxSeen = _stove.txCount();
    _flashUntil = now + 300;
  }
  uint32_t stove = _stove.isOn() ? _leds.Color(255, 80, 0) : 0;
  if (now < _flashUntil) stove = _stove.lastTxOk() ? _leds.Color(255, 255, 255) : _leds.Color(255, 0, 0);

  _leds.setPixelColor(0, net);
  _leds.setPixelColor(1, stove);
  _leds.show();
}
