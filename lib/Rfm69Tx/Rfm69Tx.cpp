#include "Rfm69Tx.h"

namespace {
// Registres SX1231 utilisés
const uint8_t REG_FIFO = 0x00;
const uint8_t REG_OPMODE = 0x01;
const uint8_t REG_DATAMODUL = 0x02;
const uint8_t REG_BITRATEMSB = 0x03;
const uint8_t REG_BITRATELSB = 0x04;
const uint8_t REG_FRFMSB = 0x07;
const uint8_t REG_FRFMID = 0x08;
const uint8_t REG_FRFLSB = 0x09;
const uint8_t REG_VERSION = 0x10;
const uint8_t REG_PALEVEL = 0x11;
const uint8_t REG_OCP = 0x13;
const uint8_t REG_IRQFLAGS1 = 0x27;
const uint8_t REG_IRQFLAGS2 = 0x28;
const uint8_t REG_PREAMBLEMSB = 0x2C;
const uint8_t REG_PREAMBLELSB = 0x2D;
const uint8_t REG_SYNCCONFIG = 0x2E;
const uint8_t REG_PACKETCONFIG1 = 0x37;
const uint8_t REG_PAYLOADLENGTH = 0x38;
const uint8_t REG_FIFOTHRESH = 0x3C;
const uint8_t REG_TESTPA1 = 0x5A;
const uint8_t REG_TESTPA2 = 0x5C;

const uint8_t MODE_SLEEP = 0x00;
const uint8_t MODE_STANDBY = 0x04;
const uint8_t MODE_TX = 0x0C;

const uint8_t IRQ1_MODEREADY = 0x80;
const uint8_t IRQ2_FIFOFULL = 0x80;
const uint8_t IRQ2_FIFONOTEMPTY = 0x40;
const uint8_t IRQ2_PACKETSENT = 0x08;

const size_t FIFO_SIZE = 66;
}  // namespace

uint8_t Rfm69Tx::readReg(uint8_t addr) {
  SPI.beginTransaction(_spi);
  digitalWrite(_cs, LOW);
  SPI.transfer(addr & 0x7F);
  uint8_t v = SPI.transfer(0);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return v;
}

void Rfm69Tx::writeReg(uint8_t addr, uint8_t value) {
  SPI.beginTransaction(_spi);
  digitalWrite(_cs, LOW);
  SPI.transfer(addr | 0x80);
  SPI.transfer(value);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void Rfm69Tx::writeFifo(const uint8_t* data, size_t len) {
  SPI.beginTransaction(_spi);
  digitalWrite(_cs, LOW);
  SPI.transfer(REG_FIFO | 0x80);
  for (size_t i = 0; i < len; i++) SPI.transfer(data[i]);
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

uint8_t Rfm69Tx::version() { return readReg(REG_VERSION); }

bool Rfm69Tx::begin() {
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  SPI.begin();

  // Le module n'a pas de broche RESET câblée sur ce shield : on vérifie qu'il
  // répond en écrivant/relisant un registre inoffensif.
  uint32_t start = millis();
  do {
    writeReg(REG_SYNCCONFIG, 0xAA);
    if (readReg(REG_SYNCCONFIG) == 0xAA) break;
    delay(5);
  } while (millis() - start < 100);
  writeReg(REG_SYNCCONFIG, 0x55);
  _present = readReg(REG_SYNCCONFIG) == 0x55;
  if (!_present) return false;

  setMode(MODE_STANDBY);
  writeReg(REG_DATAMODUL, 0x08);       // mode paquet, OOK, sans mise en forme
  writeReg(REG_PREAMBLEMSB, 0x00);     // pas de préambule
  writeReg(REG_PREAMBLELSB, 0x00);
  writeReg(REG_SYNCCONFIG, 0x00);      // pas de mot de synchro
  writeReg(REG_PACKETCONFIG1, 0x00);   // longueur fixe, pas de CRC, pas de filtrage
  writeReg(REG_FIFOTHRESH, 0x8F);      // émission dès que la FIFO n'est pas vide
  setChipDuration(_chipUs);
  applyPower();
  return true;
}

void Rfm69Tx::setFrequency(uint32_t hz) {
  // Frf = f / Fstep, Fstep = 32 MHz / 2^19
  uint32_t frf = (uint32_t)(((uint64_t)hz << 19) / 32000000ULL);
  writeReg(REG_FRFMSB, frf >> 16);
  writeReg(REG_FRFMID, frf >> 8);
  writeReg(REG_FRFLSB, frf);
}

void Rfm69Tx::setChipDuration(uint16_t us) {
  _chipUs = us;
  // Bitrate = 32 MHz / reg  =>  reg = 32 * durée_en_µs
  uint32_t reg = (uint32_t)us * 32;
  if (reg > 0xFFFF) reg = 0xFFFF;
  writeReg(REG_BITRATEMSB, reg >> 8);
  writeReg(REG_BITRATELSB, reg & 0xFF);
}

void Rfm69Tx::setModule(Module m) {
  _module = m;
  applyPower();
}

void Rfm69Tx::setPowerDbm(int8_t dbm) {
  _powerDbm = dbm;
  applyPower();
}

void Rfm69Tx::applyPower() {
  if (!_present) return;
  // Registres "haute puissance" (+20 dBm) jamais utilisés : valeurs normales.
  writeReg(REG_TESTPA1, 0x55);
  writeReg(REG_TESTPA2, 0x70);
  writeReg(REG_OCP, 0x1A);
  int8_t p = _powerDbm;
  if (_module == Module::W) {
    p = constrain(p, -18, 13);
    writeReg(REG_PALEVEL, 0x80 | (uint8_t)(p + 18));  // PA0
  } else if (p <= 13) {
    p = constrain(p, -2, 13);
    writeReg(REG_PALEVEL, 0x40 | (uint8_t)(p + 18));  // PA1
  } else {
    p = constrain(p, 2, 17);
    writeReg(REG_PALEVEL, 0x60 | (uint8_t)(p + 14));  // PA1 + PA2
  }
}

void Rfm69Tx::setMode(uint8_t mode) {
  writeReg(REG_OPMODE, mode);
  uint32_t start = millis();
  while (!(readReg(REG_IRQFLAGS1) & IRQ1_MODEREADY) && millis() - start < 50) {
  }
}

void Rfm69Tx::sleep() {
  if (_present) setMode(MODE_SLEEP);
}

bool Rfm69Tx::transmit(const uint8_t* data, size_t len) {
  if (!_present || len == 0 || len > 255) return false;

  setMode(MODE_STANDBY);
  // vide la FIFO éventuelle
  while (readReg(REG_IRQFLAGS2) & IRQ2_FIFONOTEMPTY) readReg(REG_FIFO);

  writeReg(REG_PAYLOADLENGTH, (uint8_t)len);
  size_t sent = min(len, FIFO_SIZE);
  writeFifo(data, sent);
  setMode(MODE_TX);

  // durée théorique + marge
  uint32_t timeoutMs = (uint32_t)len * 8 * _chipUs / 1000 + 200;
  uint32_t start = millis();
  bool ok = true;
  while (sent < len) {
    if (!(readReg(REG_IRQFLAGS2) & IRQ2_FIFOFULL)) {
      writeFifo(data + sent, 1);
      sent++;
    } else {
      yield();
    }
    if (millis() - start > timeoutMs) {
      ok = false;
      break;
    }
  }
  while (ok && !(readReg(REG_IRQFLAGS2) & IRQ2_PACKETSENT)) {
    if (millis() - start > timeoutMs) ok = false;
    yield();
  }
  setMode(MODE_STANDBY);
  return ok;
}
