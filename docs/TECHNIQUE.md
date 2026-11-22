# Documentation technique

Ce document s'adresse à ceux qui veulent compiler le firmware, le modifier ou comprendre son
fonctionnement. Pour une simple installation, voir le [README](../README.md).

## Organisation du code

| Dossier / fichier | Rôle |
|---|---|
| `lib/MczProtocol/` | Encodage des trames MCZ (C++ pur, testable sur PC) |
| `lib/Rfm69Tx/` | Pilote d'émission OOK du RFM69 (mode paquet) |
| `src/Settings.*` | Réglages persistants (`/config.json` dans LittleFS) |
| `src/Stove.*` | État du poêle, mémorisation (`/state.json`), émission des commandes |
| `src/Network.*` | WiFi, point d'accès de configuration avec portail captif, mDNS, OTA |
| `src/WebUi.*` | Serveur web et API REST |
| `src/HomeAssistant.*` | MQTT et découverte automatique Home Assistant |
| `src/Peripherals.*` | LEDs WS2812 et bouton du shield |
| `src/TempSensor.*` | Sonde DS18B20 optionnelle (1-Wire, lecture non bloquante) |
| `include/config.h` | Brochage, valeurs par défaut, version |
| `data/` | Interface web (HTML, CSS, JS), téléversée dans LittleFS |
| `tools/test_protocol.cpp` | Tests de l'encodeur (sur PC) |
| `tools/fs_keep_settings.py` | Conserve les réglages lors de `pio run -t uploadfs` |
| `tools/make_release.py` | Génère les fichiers de release dans `dist/` |

## Compilation

Prérequis : [PlatformIO](https://platformio.org/) (`pio`). La plateforme `espressif8266 @ 4.2.1`
est fixée dans `platformio.ini` (les versions plus anciennes ne savent pas produire d'image LittleFS).

```sh
pio run -t upload      # firmware
pio run -t uploadfs    # interface web (dossier data/)
pio device monitor     # console série, 115200 bauds
```

Les deux téléversements sont nécessaires la première fois. Ensuite, le firmware peut être mis à
jour sans câble : page **Réglages › Mettre à jour le firmware** ou `pio run -e ota -t upload`
(adapter `upload_port` dans `platformio.ini`).

### Conservation des réglages avec `uploadfs`

`uploadfs` remplace tout le système de fichiers, réglages compris. Le script
`tools/fs_keep_settings.py` lit d'abord la carte par le câble USB, récupère `config.json` (WiFi,
MQTT, mots de passe) et `state.json` (dernier état du poêle), puis les réinjecte dans la nouvelle
image. Une copie est gardée dans `.pio/fs_backup/` (mots de passe en clair, ignorée par git).
Si la lecture échoue, l'envoi est annulé. Pour envoyer `data/` seul en effaçant les réglages :
`MCZ_FS_NO_BACKUP=1 pio run -t uploadfs`. En OTA, `uploadfs` est refusé pour la même raison.

### Release

```sh
python3 tools/make_release.py
```

Produit dans `dist/` :
- `mcz-wifi-<version>-complet.bin` : firmware + interface web, à flasher à l'adresse `0x0` ;
- `mcz-wifi-<version>-firmware.bin` : firmware seul, pour la mise à jour par l'interface web.

Le script reconstruit l'image LittleFS à partir de `data/` uniquement et vérifie qu'elle ne
contient aucun réglage personnel (après un `uploadfs`, l'image du dossier `.pio` en contient).

### Tests

```sh
g++ -std=c++17 -I lib/MczProtocol lib/MczProtocol/MczProtocol.cpp tools/test_protocol.cpp -o /tmp/test_mcz && /tmp/test_mcz
```

## Matériel

Shield WeMos RFM69 de ch2i ([schémas](https://github.com/hallard/WeMos-RFM69)) :

| Fonction | ESP8266 | Wemos |
|---|---|---|
| RFM69 NSS (CS) | GPIO16 | D0 |
| RFM69 SCK | GPIO14 | D5 |
| RFM69 MISO | GPIO12 | D6 |
| RFM69 MOSI | GPIO13 | D7 |
| RFM69 DIO0 (non utilisé) | GPIO15 | D8 |
| 2 LEDs WS2812B chaînées | GPIO0 | D3 |
| Bouton (actif bas) | GPIO2 | D4 |
| Sonde DS18B20 optionnelle (DQ) | GPIO4 | D2 |

### Sonde DS18B20 (optionnelle)

Le shield (V1.0a/V1.0b) prévoit un connecteur I2C pour écran OLED (`3V3 · GND · SCL · SDA`,
SCL = GPIO5/D1, SDA = GPIO4/D2), deux emplacements de résistances de tirage 4,7 kΩ (R1 sur SDA,
R2 sur SCL) et un emplacement pour un ATSHA204A sur le même bus. Sur une carte standard, tout
cela est vide : SDA est libre et sert de ligne 1-Wire.

Câblage : résistance CMS 4,7 kΩ sur **R1** (tirage de DQ au 3,3 V), sonde sur le connecteur
(VDD → 3V3, GND → GND, DQ → SDA). Alimentation normale (pas de mode parasite). Si l'ATSHA204A
est monté, il partage la ligne : non testé dans ce cas.

Logiciel (`src/TempSensor.*`, bibliothèques OneWire et DallasTemperature) :
- recherche d'une sonde DS18B20 au démarrage, puis toutes les 60 s tant qu'il n'y en a pas ;
- résolution 12 bits, conversion lancée sans attente (`setWaitForConversion(false)`) puis lue
  750 ms plus tard : la boucle principale (WiFi, MQTT, web) n'est jamais bloquée ;
- une lecture toutes les 30 s (`TEMP_READ_INTERVAL_MS`) ; valeur arrondie au dixième, correction
  `sensor.offset` (−10 à +10 °C) ajoutée ;
- les valeurs −127 (sonde déconnectée) et 85 (valeur de mise sous tension) sont ignorées ;
  après 3 échecs consécutifs, la sonde est déclarée absente et de nouveau recherchée.

## Protocole radio

Reconstitué à partir de captures d'une vraie télécommande (SDR + Universal Radio Hacker) ;
l'encodeur reproduit bit pour bit la trame d'origine.

- OOK, Manchester, demi-bit ≈ 414 µs, sur 434,50 MHz (bande 433, version par défaut des poêles
  MCZ, fréquence donnée par la notice) ou 868,95 MHz (version 868 proposée par MCZ contre les
  interférences en 433, fréquence mesurée). Le protocole est strictement identique dans les deux
  bandes, seule la porteuse change (voir « Bandes 433 et 868 MHz » ci-dessous) ;
- trame de 7 mots de 12 bits, répétée 5 fois, ~3,7 ms de silence entre les répétitions ;
- octets : `ID(3) | FAN1:3 PUISSANCE:3 MODE:2 | BIP:1 0 FAN2:3 FAN3:3 | CRC16-XModem(2)` ;
- mot de 12 bits : `1 | octet | SOD (1 sur le premier mot) | parité paire | 1`, précédé d'un
  bit 0 et d'un demi-bit haut ;
- Manchester : 1 = haut puis bas, 0 = bas puis haut.

### Bandes 433 et 868 MHz

La fréquence se règle dans **Réglages › Radio** (préréglages 433 / 868 ou valeur libre, clé
`radio.frequency_hz` de l'API) et s'applique sans redémarrage. Le firmware la limite aux bandes
ISM autorisées : 433,05–434,79 MHz et 863–870 MHz. Puissance maximale autorisée : 10 dBm (10 mW)
en 433 MHz, 14 dBm (25 mW) en 868,7–869,2 MHz ; le préréglage 433 ramène la puissance à 10 dBm.

**Aucune différence de code entre les modules RFM69 433 et 868.** Les deux versions utilisent la
même puce (Semtech SX1231H), le même quartz de 32 MHz et les mêmes registres :

- la fréquence est programmée par `RegFrf = f / (32 MHz / 2^19)` (`Rfm69Tx::setFrequency()`) ; la
  puce choisit seule sa sous-bande (290–340, 424–510 ou 862–1020 MHz), il n'y a aucun bit de
  sélection de bande (contrairement au SX1276/RFM95 et son bit `LowFrequencyModeOn`) ;
- débit, modulation OOK, mode paquet et réglages de l'amplificateur (PA0 pour W/CW, PA1/PA2 pour
  HW/HCW) sont les mêmes dans les deux bandes ;
- seul diffère le réseau d'adaptation d'antenne (selfs et condensateurs) soudé sur le module,
  calculé pour une bande. Un module programmé hors de sa bande émet, mais l'essentiel de la
  puissance est perdu : il faut un module de la bande du poêle. Antenne quart d'onde : 17,3 cm à
  434,5 MHz, 8,2 cm à 868,95 MHz.

Bouton ON de la télécommande d'origine : mode 3, puissance 3, ventilation auto (6),
FAN2 = FAN3 = 7 (absents), BIP = 1.

- **Mode** : seul 0 (arrêt) / non nul (marche) compte. Manuel (1), Auto (2) et Eco (3) ne changent
  rien au poêle (vérifié) : la régulation Auto/Eco est faite dans la télécommande, la trame ne
  contient ni température ni consigne. Le firmware envoie 3 pour « marche », comme la télécommande.
- **BIP** : bit 7 de l'octet 5, à 1 sur les appuis de la télécommande (le poêle bipe). Les renvois
  périodiques l'envoient à 0 : le poêle ne bipe pas (vérifié).
- **Renvoi périodique** : la télécommande d'origine renvoie sa trame toutes les 5 minutes environ ;
  le firmware fait de même (réglable, 300 s par défaut).
- Le poêle n'émet rien : impossible de connaître son état réel.

### Pourquoi le mode paquet du RFM69

Sur le shield ch2i, seule la broche **DIO0** du RFM69 est reliée à l'ESP : **DIO2 n'est pas
câblée**. Or en mode OOK « continu », la donnée à émettre doit arriver sur DIO2. Le firmware
utilise donc le **mode paquet** du RFM69, sans préambule, sans mot de synchro et sans CRC : chaque
bit de la FIFO correspond à un demi-bit Manchester, le débit radio étant réglé sur 1 / 414 µs.
Le module cadence lui-même l'émission (pas de gigue due au WiFi) et aucune modification de la
carte n'est nécessaire. Le flux complet (5 répétitions) tient en 125 octets.

## API REST

| Méthode | URL | Rôle |
|---|---|---|
| GET | `/api/status` | état du poêle et du module ; `device.temperature` = `{"present":true,"celsius":20.5,"age_s":12}` |
| POST | `/api/stove` | `{"on":true,"power":3,"fan":"auto"}` (champs facultatifs) |
| POST | `/api/stove/resend` | renvoie la dernière commande |
| GET / POST | `/api/settings` | réglages (les mots de passe ne sont jamais renvoyés) |
| GET | `/api/wifi/scan` | réseaux WiFi visibles |
| POST | `/api/reboot` | redémarrage |
| POST | `/api/factory-reset` | efface tous les réglages |
| GET | `/update` | mise à jour du firmware par le navigateur |

```sh
curl http://poele-mcz.local/api/status
curl -X POST http://poele-mcz.local/api/stove -d '{"on":true,"power":4,"fan":"auto"}'
curl -X POST http://poele-mcz.local/api/stove -d '{"on":false}'
```

Avec un mot de passe web : `-u admin:motdepasse`.

## MQTT

`<base>` = `mcz/mcz-xxxxxx` par défaut (réglable).

| Topic | Contenu |
|---|---|
| `<base>/status` | `online` / `offline` (retenu, LWT) |
| `<base>/state` | `{"on":true,"power":3,"fan":"auto",...}` (retenu) |
| `<base>/diag` | `{"rssi":-41,"ip":"...","uptime_s":...}` (retenu) |
| `<base>/temperature` | température de la sonde DS18B20 en °C, ex. `20.5` (retenu, toutes les 30 s ; seulement si une sonde est branchée) |
| `<base>/set` | commande JSON, mêmes champs que l'API |
| `<base>/set/switch` | `ON` / `OFF` |
| `<base>/set/power` | `1` … `5` |
| `<base>/set/fan` | `1` … `5` / `auto` |
| `<base>/set/resend` | renvoie la dernière commande |

Découverte Home Assistant sous `<prefix>/<composant>/mcz-xxxxxx/<objet>/config` (switch, number,
select, button, sensor). Le capteur de température n'est annoncé qu'à la première détection d'une
sonde, avec `expire_after` = 120 s ; il n'est jamais retiré automatiquement. La découverte est
republiée quand HA publie `online` sur
`<prefix>/status`.
