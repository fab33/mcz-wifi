// Pilote minimal d'émission OOK pour RFM69 (SX1231), en mode paquet.
//
// Le shield WeMos RFM69 de ch2i ne relie pas la broche DIO2 du module : le mode
// OOK "continu" (donnée sur DIO2) n'est donc pas utilisable sans modification.
// On utilise à la place le mode paquet sans préambule, sans mot de synchro et
// sans CRC : chaque bit envoyé dans la FIFO correspond à un demi-bit Manchester
// (1 = porteuse). Le RFM69 cadence lui-même les bits, sans gigue due au WiFi.
#pragma once

#include <Arduino.h>
#include <SPI.h>

class Rfm69Tx {
 public:
  enum class Module : uint8_t {
    W = 0,   // RFM69W / RFM69CW : sortie PA0 (max +13 dBm)
    HW = 1,  // RFM69HW / RFM69HCW : sortie PA_BOOST (PA1/PA2, max +17 dBm ici)
  };

  explicit Rfm69Tx(uint8_t csPin) : _cs(csPin) {}

  // Initialise le module. Retourne false si le module ne répond pas.
  bool begin();
  bool present() const { return _present; }
  uint8_t version();  // 0x24 attendu pour un SX1231/RFM69

  void setFrequency(uint32_t hz);
  // Durée d'un demi-bit Manchester (= durée d'un bit radio) en microsecondes.
  void setChipDuration(uint16_t us);
  void setModule(Module m);
  void setPowerDbm(int8_t dbm);

  // Émet un flux de bits (MSB en premier, 1 = porteuse) de 255 octets maximum.
  // Bloquant pendant la durée d'émission.
  bool transmit(const uint8_t* data, size_t len);

  void sleep();

  // Pour le diagnostic.
  uint8_t readReg(uint8_t addr);
  void writeReg(uint8_t addr, uint8_t value);

 private:
  void applyPower();
  void setMode(uint8_t mode);
  void writeFifo(const uint8_t* data, size_t len);

  uint8_t _cs;
  bool _present = false;
  Module _module = Module::W;
  int8_t _powerDbm = 13;
  uint16_t _chipUs = 414;
  SPISettings _spi{4000000, MSBFIRST, SPI_MODE0};
};
