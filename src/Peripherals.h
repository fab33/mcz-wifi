// LEDs RGB et bouton du shield.
//
// LED 1 (état réseau) :  bleu clignotant = portail de configuration ouvert
//                        orange = WiFi connecté, MQTT déconnecté
//                        vert   = tout est connecté (ou MQTT désactivé)
//                        rouge  = module radio absent
// LED 2 (poêle) :        éteinte = arrêt, orange = en marche, flash blanc = émission
//
// Bouton : appui court = marche/arrêt du poêle,
//          appui long (8 s, LEDs violettes) = effacement du WiFi et redémarrage
#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class Stove;
class HomeAssistant;
class Rfm69Tx;

class Peripherals {
 public:
  Peripherals(Stove& stove, HomeAssistant& ha, Rfm69Tx& radio);
  void begin();
  void loop();

 private:
  void updateLeds();
  void handleButton();

  Stove& _stove;
  HomeAssistant& _ha;
  Rfm69Tx& _radio;
  Adafruit_NeoPixel _leds;
  uint32_t _lastLedUpdate = 0;
  uint32_t _pressedSince = 0;
  bool _lastButton = false;
  uint32_t _lastTxSeen = 0;
  uint32_t _flashUntil = 0;
};
