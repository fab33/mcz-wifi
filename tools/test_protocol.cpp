// Test hors-cible de la bibliothèque MczProtocol.
//
// L'encodeur a été validé bit à bit sur des captures radio d'une vraie
// télécommande MCZ. Pour ne pas publier l'identifiant de cette télécommande,
// les vecteurs ci-dessous utilisent un identifiant fictif (0x123456) ; ils ont
// été recalculés par une implémentation indépendante (Python).
//
//   g++ -std=c++17 -I lib/MczProtocol lib/MczProtocol/MczProtocol.cpp tools/test_protocol.cpp -o /tmp/test_mcz && /tmp/test_mcz
#include <cstdio>
#include <cstring>
#include <string>

#include "MczProtocol.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      printf("ÉCHEC %s:%d : %s\n", __FILE__, __LINE__, #cond);   \
      failures++;                                                \
    }                                                            \
  } while (0)

static const uint32_t TEST_ID = 0x123456;

struct Vector {
  const char* name;
  mcz::Command cmd;
  uint16_t words[7];
};

static mcz::Command make(mcz::Mode mode, uint8_t power, uint8_t fan1) {
  mcz::Command c;
  c.id = TEST_ID;
  c.mode = mode;
  c.power = power;
  c.fan1 = fan1;
  return c;
}

// Demi-bits d'une trame complète (bouton ON, puissance 3, ventilation auto).
static const char* ON_CHIPS =
    "0111001010110010110011010100111001011010011001010110100111001100110011010010101100111010100101"
    "10101010010110011101001101010101010011010011100101011010011001011010011101010101001010110011010";

int main() {
  const Vector vectors[] = {
      {"OFF", make(mcz::Mode::Off, 3, 6), {0x897, 0x9A3, 0xAB1, 0xE61, 0xDFB, 0xA7B, 0xD13}},
      {"ON", make(mcz::MODE_ON, 3, 6), {0x897, 0x9A3, 0xAB1, 0xE79, 0xDFB, 0x8D3, 0xF8B}},
      {"OFF 0/0", make(mcz::Mode::Off, 0, 0), {0x897, 0x9A3, 0xAB1, 0x801, 0xDFB, 0x8E3, 0xCDB}},
  };

  for (const auto& v : vectors) {
    uint8_t frame[7];
    uint16_t words[7];
    mcz::buildFrame(v.cmd, frame);
    mcz::encodeWords(frame, words);
    bool ok = memcmp(words, v.words, sizeof(words)) == 0;
    printf("%-8s %s\n", v.name, ok ? "OK" : "DIFFÉRENT");
    CHECK(ok);

    mcz::Command back;
    CHECK(mcz::decodeFrame(frame, back));
    CHECK(back.id == v.cmd.id && back.mode == v.cmd.mode && back.power == v.cmd.power &&
          back.fan1 == v.cmd.fan1 && back.fan2 == 7 && back.fan3 == 7 && back.beep);
  }

  // Flux de demi-bits envoyé à la radio.
  mcz::Timing t;
  t.repeats = 1;
  t.leadChips = 0;
  uint8_t buf[64];
  size_t n = mcz::buildChipStream(vectors[1].cmd, t, buf, sizeof(buf));
  CHECK(n == mcz::chipStreamBytes(t));
  std::string chips;
  for (size_t i = 0; i < 7 * mcz::CHIPS_PER_WORD; i++) chips += ((buf[i / 8] >> (7 - i % 8)) & 1) ? '1' : '0';
  bool same = chips == ON_CHIPS;
  printf("Flux demi-bits : %s\n", same ? "OK" : "DIFFÉRENT");
  CHECK(same);

  // Trame silencieuse : seul le bit de bip (octet 5, bit 7) change.
  {
    mcz::Command silent = vectors[1].cmd;
    silent.beep = false;
    uint8_t a[7], b[7];
    mcz::buildFrame(vectors[1].cmd, a);
    mcz::buildFrame(silent, b);
    CHECK(a[4] == 0xBF && b[4] == 0x3F);
    CHECK(memcmp(a, b, 4) == 0);
    mcz::Command back;
    CHECK(mcz::decodeFrame(b, back) && !back.beep);
  }

  // Une trame corrompue doit être rejetée.
  {
    uint8_t f[7];
    mcz::buildFrame(vectors[1].cmd, f);
    f[3] ^= 0x04;
    mcz::Command back;
    CHECK(!mcz::decodeFrame(f, back));
  }

  mcz::Timing def;
  printf("Taille du flux par défaut : %zu octets\n", mcz::chipStreamBytes(def));
  CHECK(mcz::chipStreamBytes(def) <= 255);

  printf(failures ? "\n%d échec(s)\n" : "\nTous les tests passent\n", failures);
  return failures ? 1 : 0;
}
