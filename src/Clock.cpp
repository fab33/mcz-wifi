#include "Clock.h"

#include <time.h>

#include "Settings.h"

Clock sysClock;

static const time_t VALID_AFTER = 1700000000;  // 14/11/2023 : horloge réglée

void Clock::begin() {
  configTime(settings.time.tz.c_str(), settings.time.ntp.c_str());
  Serial.printf("[Heure] Fuseau %s, serveur %s\n", settings.time.tz.c_str(), settings.time.ntp.c_str());
}

bool Clock::valid() const { return time(nullptr) > VALID_AFTER; }

bool Clock::local(uint8_t& day, uint16_t& minute) const {
  time_t now = time(nullptr);
  if (now <= VALID_AFTER) return false;
  struct tm t;
  localtime_r(&now, &t);
  day = (t.tm_wday + 6) % 7;  // tm_wday : 0 = dimanche
  minute = t.tm_hour * 60 + t.tm_min;
  return true;
}

String Clock::iso() const {
  time_t now = time(nullptr);
  if (now <= VALID_AFTER) return String();
  struct tm t;
  localtime_r(&now, &t);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
  return String(buf);
}

uint32_t Clock::uptimeS() {
  uint32_t now = millis();
  _remMs += now - _lastMs;  // correct même au retour à zéro de millis()
  _lastMs = now;
  _secs += _remMs / 1000;
  _remMs %= 1000;
  return _secs;
}
