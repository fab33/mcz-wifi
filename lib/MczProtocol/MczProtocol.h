// Protocole radio des télécommandes MCZ (433 ou 868 MHz, OOK, codage Manchester).
//
// Bibliothèque C++ pure (aucune dépendance Arduino) : elle peut être testée sur PC
// (voir test/native). Format validé sur les captures de la vraie télécommande
// (Ressources/Analyse MCZ) :
//
//   Trame = 7 mots de 12 bits, répétée 5 fois.
//   Octets : ID[23:16] ID[15:8] ID[7:0] P1 P2 CRC_H CRC_L
//     P1 = FAN1(3 bits) | PUISSANCE(3 bits) | MODE(2 bits)
//     P2 = BIP(1 bit) | 0 | FAN2(3 bits) | FAN3(3 bits, 7 = absent)
//          BIP = 1 sur les appuis de la télécommande (le poêle bipe à la
//          réception) ; mis à 0 pour les renvois périodiques silencieux (vérifié).
//     CRC = CRC16 XModem (poly 0x1021, init 0) sur les 5 premiers octets
//   Mot 12 bits : 1 | octet(8) | SOD (1 sur le 1er mot) | parité paire | 1
//   Chaque mot est précédé d'un bit 0 de préambule suivi d'un demi-bit haut.
//   Manchester : 1 = haut puis bas, 0 = bas puis haut.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mcz {

// Le mode n'a aucun effet sur le poêle (vérifié) : la régulation Auto/Eco est
// faite dans la télécommande, qui envoie ensuite puissance et ventilation.
// Seule la distinction arrêt / marche compte.
enum class Mode : uint8_t {
  Off = 0,
  Manual = 1,
  Auto = 2,
  Eco = 3,  // valeur envoyée par la télécommande d'origine à l'allumage
};
static const Mode MODE_ON = Mode::Eco;

static const uint8_t FAN_AUTO = 6;     // ventilateur en automatique
static const uint8_t FAN_ABSENT = 7;   // ventilateur 2 ou 3 absent (poêle à 1 ventilateur)
static const uint8_t POWER_MIN = 1;
static const uint8_t POWER_MAX = 5;

struct Command {
  uint32_t id = 0;       // identifiant 24 bits de la télécommande appairée
  Mode mode = Mode::Off;
  uint8_t power = 3;     // 1..5 (3 bits)
  uint8_t fan1 = FAN_AUTO;  // 0..5, 6 = auto (3 bits)
  uint8_t fan2 = FAN_ABSENT;
  uint8_t fan3 = FAN_ABSENT;
  bool beep = true;      // le poêle bipe à la réception
};

static const size_t FRAME_BYTES = 7;
static const size_t FRAME_WORDS = 7;
static const size_t CHIPS_PER_WORD = 27;  // 3 demi-bits de préambule + 12 bits x 2

// Paramètres de mise en forme du flux radio.
struct Timing {
  uint8_t repeats = 5;     // nombre de répétitions de la trame
  uint8_t gapChips = 9;    // silence entre deux trames, en demi-bits (~3,7 ms)
  uint8_t leadChips = 8;   // silence avant la première trame
};

uint16_t crc16XModem(const uint8_t* data, size_t len);

// Construit les 7 octets (données + CRC).
void buildFrame(const Command& cmd, uint8_t out[FRAME_BYTES]);

// Encapsule les 7 octets en 7 mots de 12 bits.
void encodeWords(const uint8_t frame[FRAME_BYTES], uint16_t words[FRAME_WORDS]);

// Taille (octets) du flux de demi-bits produit par buildChipStream().
size_t chipStreamBytes(const Timing& t);

// Produit le flux complet de demi-bits ("chips"), 1 = porteuse, MSB en premier,
// prêt à être envoyé tel quel par le RFM69 en mode paquet OOK avec un débit
// d'un bit par demi-bit Manchester. Retourne le nombre d'octets écrits (0 si
// le tampon est trop petit).
size_t buildChipStream(const Command& cmd, const Timing& t, uint8_t* out, size_t outSize);

// Décodage inverse (utile pour les tests et le diagnostic).
bool decodeFrame(const uint8_t frame[FRAME_BYTES], Command& cmd);

const char* modeName(Mode m);

}  // namespace mcz
