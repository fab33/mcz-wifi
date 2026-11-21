#include "MczProtocol.h"

#include <string.h>

namespace mcz {

uint16_t crc16XModem(const uint8_t* data, size_t len) {
  uint16_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

void buildFrame(const Command& cmd, uint8_t out[FRAME_BYTES]) {
  out[0] = (cmd.id >> 16) & 0xFF;
  out[1] = (cmd.id >> 8) & 0xFF;
  out[2] = cmd.id & 0xFF;
  out[3] = ((cmd.fan1 & 0x07) << 5) | ((cmd.power & 0x07) << 2) | ((uint8_t)cmd.mode & 0x03);
  out[4] = (cmd.beep ? 0x80 : 0x00) | ((cmd.fan2 & 0x07) << 3) | (cmd.fan3 & 0x07);
  uint16_t crc = crc16XModem(out, 5);
  out[5] = crc >> 8;
  out[6] = crc & 0xFF;
}

static uint8_t parity(uint16_t v) {
  uint8_t p = 0;
  while (v) {
    p ^= v & 1;
    v >>= 1;
  }
  return p;
}

void encodeWords(const uint8_t frame[FRAME_BYTES], uint16_t words[FRAME_WORDS]) {
  for (size_t i = 0; i < FRAME_WORDS; i++) {
    uint16_t inter = ((uint16_t)frame[i] << 1) | (i == 0 ? 1 : 0);  // octet + SOD
    words[i] = (1 << 11) | (inter << 2) | (parity(inter) << 1) | 1;
  }
}

size_t chipStreamBytes(const Timing& t) {
  size_t chips = t.leadChips + (size_t)t.repeats * (FRAME_WORDS * CHIPS_PER_WORD + t.gapChips);
  return (chips + 7) / 8;
}

namespace {
struct BitWriter {
  explicit BitWriter(uint8_t* b) : buf(b), pos(0) {}
  uint8_t* buf;
  size_t pos;  // en bits
  void put(bool bit) {
    if (bit) buf[pos >> 3] |= 0x80 >> (pos & 7);
    pos++;
  }
  void zeros(size_t n) { pos += n; }
};
}  // namespace

size_t buildChipStream(const Command& cmd, const Timing& t, uint8_t* out, size_t outSize) {
  size_t needed = chipStreamBytes(t);
  if (outSize < needed) return 0;
  memset(out, 0, needed);

  uint8_t frame[FRAME_BYTES];
  uint16_t words[FRAME_WORDS];
  buildFrame(cmd, frame);
  encodeWords(frame, words);

  BitWriter w(out);
  w.zeros(t.leadChips);
  for (uint8_t r = 0; r < t.repeats; r++) {
    for (size_t i = 0; i < FRAME_WORDS; i++) {
      // bit 0 de préambule (bas, haut) + demi-bit haut supplémentaire
      w.put(false);
      w.put(true);
      w.put(true);
      for (int b = 11; b >= 0; b--) {
        bool bit = (words[i] >> b) & 1;
        w.put(bit);
        w.put(!bit);
      }
    }
    w.zeros(t.gapChips);
  }
  return needed;
}

bool decodeFrame(const uint8_t frame[FRAME_BYTES], Command& cmd) {
  uint16_t crc = crc16XModem(frame, 5);
  if (frame[5] != (crc >> 8) || frame[6] != (crc & 0xFF)) return false;
  cmd.id = ((uint32_t)frame[0] << 16) | ((uint32_t)frame[1] << 8) | frame[2];
  cmd.fan1 = frame[3] >> 5;
  cmd.power = (frame[3] >> 2) & 0x07;
  cmd.mode = (Mode)(frame[3] & 0x03);
  cmd.beep = frame[4] & 0x80;
  cmd.fan2 = (frame[4] >> 3) & 0x07;
  cmd.fan3 = frame[4] & 0x07;
  return true;
}

static const char* const MODE_NAMES[] = {"off", "manual", "auto", "eco"};

const char* modeName(Mode m) { return MODE_NAMES[(uint8_t)m & 0x03]; }

}  // namespace mcz
