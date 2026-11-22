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
| `lib/Thermostat/` | Programmation hebdomadaire et régulation du mode autonome (C++ pur, testable sur PC) |
| `src/Clock.*` | Heure locale par NTP, fuseau POSIX |
| `src/Autonomous.*` | Mode autonome : `/schedule.json`, régulation, commandes au poêle |
| `include/config.h` | Brochage, valeurs par défaut, version |
| `data/` | Interface web (HTML, CSS, JS), téléversée dans LittleFS |
| `tools/test_protocol.cpp` | Tests de l'encodeur (sur PC) |
| `tools/test_thermostat.cpp` | Tests de la programmation et de la régulation (sur PC) |
| `tools/make_demo.py`, `tools/demo/demo.js` | Démo de l'interface sans module (faux module en JavaScript) |
| `tools/make_screenshots.py`, `tools/demo/shot.js` | Captures d'écran de la documentation (`docs/images/`) |
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
jour sans câble : page **Réglages › Mettre à jour** ou `pio run -e ota -t upload`
(adapter `upload_port` dans `platformio.ini`).

La page `/update` accepte aussi l'image LittleFS de l'interface (`…-interface.bin`, champ
`filesystem`). Cette image remplace tout le système de fichiers. Le firmware lit donc d'abord
`config.json`, `state.json` et `schedule.json` en mémoire, écrit l'image, puis les réécrit dans
le nouveau système de fichiers avant de redémarrer. En cas d'échec de l'envoi, ils sont aussi
réécrits (le système de fichiers est reformaté s'il est illisible).

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
g++ -std=c++17 -I lib/Thermostat lib/Thermostat/Thermostat.cpp tools/test_thermostat.cpp -o /tmp/test_thermo && /tmp/test_thermo
```

### Démo de l'interface

```sh
python3 tools/make_demo.py        # -> build/demo/index.html, à ouvrir dans un navigateur
```

Les pages de `data/` sont copiées sans modification ; `tools/demo/demo.js`, chargé avant
`common.js`, remplace `fetch()` par un faux module : poêle, pièce (pertes vers 8 °C, constante de
temps 2 h, apport proportionnel à la puissance) et mode autonome (même logique que
`lib/Thermostat`), avec temps accéléré réglable. L'état est gardé dans le `localStorage`.
`--artifact` produit une `index.html` sans squelette HTML pour un hébergement qui ajoute le sien.

Les captures d'écran de la documentation (`docs/images/`) sont générées à partir de cette démo,
dans un état fixe (`tools/demo/shot.js`), avec Chrome ou Chromium sans écran :

```sh
python3 tools/make_screenshots.py
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
  périodiques l'envoient à 0 : le poêle ne bipe pas (vérifié). Le firmware ne le met à 1 que
  pour une trame qui passe de marche à arrêt ou inversement (`Stove::send()`) : changements de
  puissance ou de ventilation et renvois (y compris « Renvoyer la commande ») sont silencieux ; le poêle
  applique bien un changement de puissance reçu sans bip (vérifié).
- **Réglages poêle arrêté** : poêle arrêté, un changement de puissance ou de ventilation n'est
  pas émis, seulement mémorisé (appliqué à l'allumage). L'émettre fait biper le poêle pour rien
  (constaté), et l'effet d'une trame « arrêt » dont la puissance change n'est pas connu. Toute
  trame « arrêt » reprend aussi la puissance et la ventilation de la dernière trame émise
  (`sent_power`, `sent_fan` dans `state.json`).
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

## Mode autonome

- **Heure** : `configTime()` avec le fuseau POSIX `time.tz` (par défaut
  `CET-1CEST,M3.5.0,M10.5.0/3`) et le serveur `time.ntp`. Une fois synchronisée, l'heure continue
  de tourner sans réseau (nouvelle synchronisation SNTP toutes les heures, l'heure locale est
  gardée en cas d'échec).
- **Mode dégradé** (heure inconnue, typiquement un redémarrage sans accès au serveur de temps) :
  la consigne appliquée avec une heure valide est enregistrée (`last_target` dans
  `/schedule.json`) ; sans heure, la régulation tourne sur cette consigne avec
  `allowStart = false` : chauffe, maintien et arrêt fonctionnent, mais la phase Arrêt ne mène
  jamais à l'Allumage. La programmation est suspendue jusqu'au retour de l'heure. Sans consigne
  enregistrée, rien n'est fait. `/api/status` indique `"degraded": true`.
- **Programmation** (`lib/Thermostat`, `thermo::Week`) : 7 jours (0 = lundi), 12 plages au plus
  par jour, `[début, fin[` en minutes (fin 1440 = minuit), consigne en dixièmes de °C (5 à 30 °C)
  ou arrêt. Les plages d'une journée ne se chevauchent pas (refusé par l'API). En dehors des
  plages : arrêt. Enregistrée avec l'état actif/inactif dans `/schedule.json`, conservé par
  `uploadfs` et effacé par la réinitialisation.
- **Régulation** (`thermo::Regulator`), exécutée toutes les 5 s :
  - Arrêt → Allumage si T ≤ consigne − écart (`auto.hysteresis`, 1 °C) ;
  - Allumage : `auto.startup_power` (5) pendant `auto.startup_min` (20 min), puis Maintien si
    T ≥ consigne, sinon Chauffe ;
  - Chauffe : `auto.heat_power` (3) jusqu'à T ≥ consigne, puis Maintien ;
  - Maintien : `auto.hold_power` (1) pendant `auto.hold_min` (15 min) ; à la fin, si
    T < T(début du maintien) − 0,1 °C → Chauffe, sinon → Arrêt ;
  - plage arrêt / pas de plage → Arrêt immédiat ; sonde muette plus de 5 min → Arrêt ;
  - à l'activation (et au démarrage du module), si le poêle est déjà allumé, la régulation
    commence en Chauffe au lieu de l'éteindre.
- **Commandes** : le module émet lorsque l'état voulu (marche, puissance, ventilation auto)
  diffère de l'état courant ; bip à l'allumage et à l'arrêt seulement, si `auto.beep`. À la première régulation après le
  démarrage du module ou l'activation du mode, l'état voulu est émis même s'il n'a pas changé
  (sans bip). Ensuite, comme pour toute commande, l'état est renvoyé sans bip toutes les
  `stove.resend_s` secondes (300 par défaut) par `Stove::loop()`.
- **Reprise manuelle** : `Stove::applyAutomatic()` ne déclenche pas les écouteurs « manuels » ;
  toute autre modification (`setOn`, `setPower`, `setFan`, `applyJson`) désactive le mode autonome.
- Horloge monotone en secondes (`Clock::uptimeS()`), insensible au retour à zéro de `millis()`.

## API REST

| Méthode | URL | Rôle |
|---|---|---|
| GET | `/api/status` | état du poêle et du module ; `device.temperature` = `{"present":true,"celsius":20.5,"age_s":12}` ; `auto` = mode autonome (`enabled`, `time`, `time_valid`, `day`, `minute`, `target`, `next`, `phase`, `power`, `phase_elapsed_s`, `phase_duration_s`) |
| POST | `/api/stove` | `{"on":true,"power":3,"fan":"auto"}` (champs facultatifs ; désactive le mode autonome) |
| POST | `/api/stove/resend` | renvoie la dernière commande |
| POST | `/api/auto` | `{"enabled":true}` active / désactive le mode autonome |
| GET / POST | `/api/schedule` | programmation : `{"days":[[{"start":360,"end":480,"target":20.5}, …], … 7 jours]}`, `target: null` = arrêt ; le POST remplace tout |
| GET / POST | `/api/settings` | réglages (les mots de passe ne sont jamais renvoyés) |
| GET | `/api/wifi/scan` | réseaux WiFi visibles |
| POST | `/api/reboot` | redémarrage |
| POST | `/api/factory-reset` | efface tous les réglages |
| GET | `/update` | page de mise à jour (firmware ou interface web) |
| POST | `/update` | fichier `firmware` ou `filesystem` (réglages conservés) |

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
| `<base>/auto` | mode autonome, même contenu que `auto` de `/api/status` (retenu) |
| `<base>/set/auto` | `ON` / `OFF` : active / désactive le mode autonome |

Découverte Home Assistant sous `<prefix>/<composant>/mcz-xxxxxx/<objet>/config` (switch, number,
select, button, sensor). Le capteur de température n'est annoncé qu'à la première détection d'une
sonde, avec `expire_after` = 120 s ; il n'est jamais retiré automatiquement. La découverte est
republiée quand HA publie `online` sur
`<prefix>/status`.
